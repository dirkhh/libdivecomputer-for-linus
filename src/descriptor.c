/*
 * libdivecomputer
 *
 * Copyright (C) 2012 Jef Driesen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "descriptor-private.h"
#include "iterator-private.h"
#include "platform.h"

#define C_ARRAY_SIZE(array) (sizeof (array) / sizeof *(array))

static int dc_filter_uwatec (dc_transport_t transport, const void *userdata);
static int dc_filter_suunto (dc_transport_t transport, const void *userdata);
static int dc_filter_shearwater (dc_transport_t transport, const void *userdata);
static int dc_filter_hw (dc_transport_t transport, const void *userdata);
static int dc_filter_tecdiving (dc_transport_t transport, const void *userdata);
static int dc_filter_garmin (dc_transport_t transport, const void *userdata);

static dc_status_t dc_descriptor_iterator_next (dc_iterator_t *iterator, void *item);

struct dc_descriptor_t {
	const char *vendor;
	const char *product;
	dc_family_t type;
	unsigned int model;
	unsigned int transports;
	dc_filter_t filter;
};

typedef struct dc_descriptor_iterator_t {
	dc_iterator_t base;
	size_t current;
} dc_descriptor_iterator_t;

static const dc_iterator_vtable_t dc_descriptor_iterator_vtable = {
	sizeof(dc_descriptor_iterator_t),
	dc_descriptor_iterator_next,
	NULL,
};

/*
 * The model numbers in the table are the actual model numbers reported by the
 * device. For devices where there is no model number available (or known), an
 * artifical number (starting at zero) is assigned.  If the model number isn't
 * actually used to identify individual models, identical values are assigned.
 */

static const dc_descriptor_t g_descriptors[] = {
	/* Suunto Solution */
	{"Suunto", "Solution", DC_FAMILY_SUUNTO_SOLUTION, 0, DC_TRANSPORT_SERIAL, NULL},
	/* Suunto Eon */
	{"Suunto", "Eon",             DC_FAMILY_SUUNTO_EON, 0, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "Solution Alpha",  DC_FAMILY_SUUNTO_EON, 0, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "Solution Nitrox", DC_FAMILY_SUUNTO_EON, 0, DC_TRANSPORT_SERIAL, NULL},
	/* Suunto Vyper */
	{"Suunto", "Spyder",   DC_FAMILY_SUUNTO_VYPER, 0x01, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "Stinger",  DC_FAMILY_SUUNTO_VYPER, 0x03, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "Mosquito", DC_FAMILY_SUUNTO_VYPER, 0x04, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "D3",       DC_FAMILY_SUUNTO_VYPER, 0x05, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "Vyper",    DC_FAMILY_SUUNTO_VYPER, 0x0A, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "Vytec",    DC_FAMILY_SUUNTO_VYPER, 0X0B, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "Cobra",    DC_FAMILY_SUUNTO_VYPER, 0X0C, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "Gekko",    DC_FAMILY_SUUNTO_VYPER, 0X0D, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "Zoop",     DC_FAMILY_SUUNTO_VYPER, 0x16, DC_TRANSPORT_SERIAL, NULL},
	/* Suunto Vyper 2 */
	{"Suunto", "Vyper 2",   DC_FAMILY_SUUNTO_VYPER2, 0x10, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "Cobra 2",   DC_FAMILY_SUUNTO_VYPER2, 0x11, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "Vyper Air", DC_FAMILY_SUUNTO_VYPER2, 0x13, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "Cobra 3",   DC_FAMILY_SUUNTO_VYPER2, 0x14, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "HelO2",     DC_FAMILY_SUUNTO_VYPER2, 0x15, DC_TRANSPORT_SERIAL, NULL},
	/* Suunto D9 */
	{"Suunto", "D9",         DC_FAMILY_SUUNTO_D9, 0x0E, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "D6",         DC_FAMILY_SUUNTO_D9, 0x0F, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "D4",         DC_FAMILY_SUUNTO_D9, 0x12, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "D4i",        DC_FAMILY_SUUNTO_D9, 0x19, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "D6i",        DC_FAMILY_SUUNTO_D9, 0x1A, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "D9tx",       DC_FAMILY_SUUNTO_D9, 0x1B, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "DX",         DC_FAMILY_SUUNTO_D9, 0x1C, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "Vyper Novo", DC_FAMILY_SUUNTO_D9, 0x1D, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "Zoop Novo",  DC_FAMILY_SUUNTO_D9, 0x1E, DC_TRANSPORT_SERIAL, NULL},
	{"Suunto", "D4f",        DC_FAMILY_SUUNTO_D9, 0x20, DC_TRANSPORT_SERIAL, NULL},
	/* Suunto EON Steel */
	{"Suunto", "EON Steel", DC_FAMILY_SUUNTO_EONSTEEL, 0, DC_TRANSPORT_USBHID | DC_TRANSPORT_BLE, dc_filter_suunto},
	{"Suunto", "EON Core",  DC_FAMILY_SUUNTO_EONSTEEL, 1, DC_TRANSPORT_USBHID | DC_TRANSPORT_BLE, dc_filter_suunto},
	/* Uwatec Aladin */
	{"Uwatec", "Aladin Air Twin",     DC_FAMILY_UWATEC_ALADIN, 0x1C, DC_TRANSPORT_SERIAL, NULL},
	{"Uwatec", "Aladin Sport Plus",   DC_FAMILY_UWATEC_ALADIN, 0x3E, DC_TRANSPORT_SERIAL, NULL},
	{"Uwatec", "Aladin Pro",          DC_FAMILY_UWATEC_ALADIN, 0x3F, DC_TRANSPORT_SERIAL, NULL},
	{"Uwatec", "Aladin Air Z",        DC_FAMILY_UWATEC_ALADIN, 0x44, DC_TRANSPORT_SERIAL, NULL},
	{"Uwatec", "Aladin Air Z O2",     DC_FAMILY_UWATEC_ALADIN, 0xA4, DC_TRANSPORT_SERIAL, NULL},
	{"Uwatec", "Aladin Air Z Nitrox", DC_FAMILY_UWATEC_ALADIN, 0xF4, DC_TRANSPORT_SERIAL, NULL},
	{"Uwatec", "Aladin Pro Ultra",    DC_FAMILY_UWATEC_ALADIN, 0xFF, DC_TRANSPORT_SERIAL, NULL},
	/* Uwatec Memomouse */
	{"Uwatec", "Memomouse", DC_FAMILY_UWATEC_MEMOMOUSE, 0, DC_TRANSPORT_SERIAL, NULL},
	/* Uwatec Smart */
	{"Uwatec",   "Smart Pro",           DC_FAMILY_UWATEC_SMART, 0x10, DC_TRANSPORT_IRDA, dc_filter_uwatec},
	{"Uwatec",   "Galileo Sol",         DC_FAMILY_UWATEC_SMART, 0x11, DC_TRANSPORT_IRDA, dc_filter_uwatec},
	{"Uwatec",   "Galileo Luna",        DC_FAMILY_UWATEC_SMART, 0x11, DC_TRANSPORT_IRDA, dc_filter_uwatec},
	{"Uwatec",   "Galileo Terra",       DC_FAMILY_UWATEC_SMART, 0x11, DC_TRANSPORT_IRDA, dc_filter_uwatec},
	{"Uwatec",   "Aladin Tec",          DC_FAMILY_UWATEC_SMART, 0x12, DC_TRANSPORT_IRDA, dc_filter_uwatec},
	{"Uwatec",   "Aladin Prime",        DC_FAMILY_UWATEC_SMART, 0x12, DC_TRANSPORT_IRDA, dc_filter_uwatec},
	{"Uwatec",   "Aladin Tec 2G",       DC_FAMILY_UWATEC_SMART, 0x13, DC_TRANSPORT_IRDA, dc_filter_uwatec},
	{"Uwatec",   "Aladin 2G",           DC_FAMILY_UWATEC_SMART, 0x13, DC_TRANSPORT_IRDA, dc_filter_uwatec},
	{"Subgear",  "XP-10",               DC_FAMILY_UWATEC_SMART, 0x13, DC_TRANSPORT_IRDA, dc_filter_uwatec},
	{"Uwatec",   "Smart Com",           DC_FAMILY_UWATEC_SMART, 0x14, DC_TRANSPORT_IRDA, dc_filter_uwatec},
	{"Uwatec",   "Aladin 2G",           DC_FAMILY_UWATEC_SMART, 0x15, DC_TRANSPORT_IRDA, dc_filter_uwatec},
	{"Uwatec",   "Aladin Tec 3G",       DC_FAMILY_UWATEC_SMART, 0x15, DC_TRANSPORT_IRDA, dc_filter_uwatec},
	{"Uwatec",   "Aladin Sport",        DC_FAMILY_UWATEC_SMART, 0x15, DC_TRANSPORT_IRDA, dc_filter_uwatec},
	{"Subgear",  "XP-3G",               DC_FAMILY_UWATEC_SMART, 0x15, DC_TRANSPORT_IRDA, dc_filter_uwatec},
	{"Scubapro", "Aladin Sport Matrix", DC_FAMILY_UWATEC_SMART, 0x17, DC_TRANSPORT_BLE, dc_filter_uwatec},
	{"Uwatec",   "Smart Tec",           DC_FAMILY_UWATEC_SMART, 0x18, DC_TRANSPORT_IRDA, dc_filter_uwatec},
	{"Uwatec",   "Galileo Trimix",      DC_FAMILY_UWATEC_SMART, 0x19, DC_TRANSPORT_IRDA, dc_filter_uwatec},
	{"Uwatec",   "Smart Z",             DC_FAMILY_UWATEC_SMART, 0x1C, DC_TRANSPORT_IRDA, dc_filter_uwatec},
	{"Subgear",  "XP Air",              DC_FAMILY_UWATEC_SMART, 0x1C, DC_TRANSPORT_IRDA, dc_filter_uwatec},
	{"Scubapro", "Meridian",            DC_FAMILY_UWATEC_SMART, 0x20, DC_TRANSPORT_SERIAL, NULL},
	{"Scubapro", "Mantis",              DC_FAMILY_UWATEC_SMART, 0x20, DC_TRANSPORT_SERIAL, NULL},
	{"Scubapro", "Aladin Square",       DC_FAMILY_UWATEC_SMART, 0x22, DC_TRANSPORT_USBHID, dc_filter_uwatec},
	{"Scubapro", "Chromis",             DC_FAMILY_UWATEC_SMART, 0x24, DC_TRANSPORT_SERIAL, NULL},
	{"Scubapro", "Mantis 2",            DC_FAMILY_UWATEC_SMART, 0x26, DC_TRANSPORT_SERIAL, NULL},
	{"Scubapro", "G2",                  DC_FAMILY_UWATEC_SMART, 0x32, DC_TRANSPORT_USBHID | DC_TRANSPORT_BLE, dc_filter_uwatec},
	{"Scubapro", "G2 Console",          DC_FAMILY_UWATEC_SMART, 0x32, DC_TRANSPORT_USBHID | DC_TRANSPORT_BLE, dc_filter_uwatec},
	/* Reefnet */
	{"Reefnet", "Sensus",       DC_FAMILY_REEFNET_SENSUS, 1, DC_TRANSPORT_SERIAL, NULL},
	{"Reefnet", "Sensus Pro",   DC_FAMILY_REEFNET_SENSUSPRO, 2, DC_TRANSPORT_SERIAL, NULL},
	{"Reefnet", "Sensus Ultra", DC_FAMILY_REEFNET_SENSUSULTRA, 3, DC_TRANSPORT_SERIAL, NULL},
	/* Oceanic VT Pro */
	{"Aeris",    "500 AI",     DC_FAMILY_OCEANIC_VTPRO, 0x4151, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "Versa Pro",  DC_FAMILY_OCEANIC_VTPRO, 0x4155, DC_TRANSPORT_SERIAL, NULL},
	{"Aeris",    "Atmos 2",    DC_FAMILY_OCEANIC_VTPRO, 0x4158, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "Pro Plus 2", DC_FAMILY_OCEANIC_VTPRO, 0x4159, DC_TRANSPORT_SERIAL, NULL},
	{"Aeris",    "Atmos AI",   DC_FAMILY_OCEANIC_VTPRO, 0x4244, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "VT Pro",     DC_FAMILY_OCEANIC_VTPRO, 0x4245, DC_TRANSPORT_SERIAL, NULL},
	{"Sherwood", "Wisdom",     DC_FAMILY_OCEANIC_VTPRO, 0x4246, DC_TRANSPORT_SERIAL, NULL},
	{"Aeris",    "Elite",      DC_FAMILY_OCEANIC_VTPRO, 0x424F, DC_TRANSPORT_SERIAL, NULL},
	/* Oceanic Veo 250 */
	{"Genesis", "React Pro", DC_FAMILY_OCEANIC_VEO250, 0x4247, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic", "Veo 200",   DC_FAMILY_OCEANIC_VEO250, 0x424B, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic", "Veo 250",   DC_FAMILY_OCEANIC_VEO250, 0x424C, DC_TRANSPORT_SERIAL, NULL},
	{"Seemann", "XP5",       DC_FAMILY_OCEANIC_VEO250, 0x4251, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic", "Veo 180",   DC_FAMILY_OCEANIC_VEO250, 0x4252, DC_TRANSPORT_SERIAL, NULL},
	{"Aeris",   "XR-2",      DC_FAMILY_OCEANIC_VEO250, 0x4255, DC_TRANSPORT_SERIAL, NULL},
	{"Sherwood", "Insight",  DC_FAMILY_OCEANIC_VEO250, 0x425A, DC_TRANSPORT_SERIAL, NULL},
	{"Hollis",  "DG02",      DC_FAMILY_OCEANIC_VEO250, 0x4352, DC_TRANSPORT_SERIAL, NULL},
	/* Oceanic Atom 2.0 */
	{"Oceanic",  "Atom 1.0",            DC_FAMILY_OCEANIC_ATOM2, 0x4250, DC_TRANSPORT_SERIAL, NULL},
	{"Aeris",    "Epic",                DC_FAMILY_OCEANIC_ATOM2, 0x4257, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "VT3",                 DC_FAMILY_OCEANIC_ATOM2, 0x4258, DC_TRANSPORT_SERIAL, NULL},
	{"Aeris",    "Elite T3",            DC_FAMILY_OCEANIC_ATOM2, 0x4259, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "Atom 2.0",            DC_FAMILY_OCEANIC_ATOM2, 0x4342, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "Geo",                 DC_FAMILY_OCEANIC_ATOM2, 0x4344, DC_TRANSPORT_SERIAL, NULL},
	{"Aeris",    "Manta",               DC_FAMILY_OCEANIC_ATOM2, 0x4345, DC_TRANSPORT_SERIAL, NULL},
	{"Aeris",    "XR-1 NX",             DC_FAMILY_OCEANIC_ATOM2, 0x4346, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "Datamask",            DC_FAMILY_OCEANIC_ATOM2, 0x4347, DC_TRANSPORT_SERIAL, NULL},
	{"Aeris",    "Compumask",           DC_FAMILY_OCEANIC_ATOM2, 0x4348, DC_TRANSPORT_SERIAL, NULL},
	{"Aeris",    "F10",                 DC_FAMILY_OCEANIC_ATOM2, 0x434D, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "OC1",                 DC_FAMILY_OCEANIC_ATOM2, 0x434E, DC_TRANSPORT_SERIAL, NULL},
	{"Sherwood", "Wisdom 2",            DC_FAMILY_OCEANIC_ATOM2, 0x4350, DC_TRANSPORT_SERIAL, NULL},
	{"Sherwood", "Insight 2",           DC_FAMILY_OCEANIC_ATOM2, 0x4353, DC_TRANSPORT_SERIAL, NULL},
	{"Genesis",  "React Pro White",     DC_FAMILY_OCEANIC_ATOM2, 0x4354, DC_TRANSPORT_SERIAL, NULL},
	{"Tusa",     "Element II (IQ-750)", DC_FAMILY_OCEANIC_ATOM2, 0x4357, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "Veo 1.0",             DC_FAMILY_OCEANIC_ATOM2, 0x4358, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "Veo 2.0",             DC_FAMILY_OCEANIC_ATOM2, 0x4359, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "Veo 3.0",             DC_FAMILY_OCEANIC_ATOM2, 0x435A, DC_TRANSPORT_SERIAL, NULL},
	{"Tusa",     "Zen (IQ-900)",        DC_FAMILY_OCEANIC_ATOM2, 0x4441, DC_TRANSPORT_SERIAL, NULL},
	{"Tusa",     "Zen Air (IQ-950)",    DC_FAMILY_OCEANIC_ATOM2, 0x4442, DC_TRANSPORT_SERIAL, NULL},
	{"Aeris",    "Atmos AI 2",          DC_FAMILY_OCEANIC_ATOM2, 0x4443, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "Pro Plus 2.1",        DC_FAMILY_OCEANIC_ATOM2, 0x4444, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "Geo 2.0",             DC_FAMILY_OCEANIC_ATOM2, 0x4446, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "VT4",                 DC_FAMILY_OCEANIC_ATOM2, 0x4447, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "OC1",                 DC_FAMILY_OCEANIC_ATOM2, 0x4449, DC_TRANSPORT_SERIAL, NULL},
	{"Beuchat",  "Voyager 2G",          DC_FAMILY_OCEANIC_ATOM2, 0x444B, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "Atom 3.0",            DC_FAMILY_OCEANIC_ATOM2, 0x444C, DC_TRANSPORT_SERIAL, NULL},
	{"Hollis",   "DG03",                DC_FAMILY_OCEANIC_ATOM2, 0x444D, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "OCS",                 DC_FAMILY_OCEANIC_ATOM2, 0x4450, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "OC1",                 DC_FAMILY_OCEANIC_ATOM2, 0x4451, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "VT 4.1",              DC_FAMILY_OCEANIC_ATOM2, 0x4452, DC_TRANSPORT_SERIAL, NULL},
	{"Aeris",    "Epic",                DC_FAMILY_OCEANIC_ATOM2, 0x4453, DC_TRANSPORT_SERIAL, NULL},
	{"Aeris",    "Elite T3",            DC_FAMILY_OCEANIC_ATOM2, 0x4455, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "Atom 3.1",            DC_FAMILY_OCEANIC_ATOM2, 0x4456, DC_TRANSPORT_SERIAL, NULL},
	{"Aeris",    "A300 AI",             DC_FAMILY_OCEANIC_ATOM2, 0x4457, DC_TRANSPORT_SERIAL, NULL},
	{"Sherwood", "Wisdom 3",            DC_FAMILY_OCEANIC_ATOM2, 0x4458, DC_TRANSPORT_SERIAL, NULL},
	{"Aeris",    "A300",                DC_FAMILY_OCEANIC_ATOM2, 0x445A, DC_TRANSPORT_SERIAL, NULL},
	{"Hollis",   "TX1",                 DC_FAMILY_OCEANIC_ATOM2, 0x4542, DC_TRANSPORT_SERIAL, NULL},
	{"Beuchat",  "Mundial 2",           DC_FAMILY_OCEANIC_ATOM2, 0x4543, DC_TRANSPORT_SERIAL, NULL},
	{"Sherwood", "Amphos",              DC_FAMILY_OCEANIC_ATOM2, 0x4545, DC_TRANSPORT_SERIAL, NULL},
	{"Sherwood", "Amphos Air",          DC_FAMILY_OCEANIC_ATOM2, 0x4546, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "Pro Plus 3",          DC_FAMILY_OCEANIC_ATOM2, 0x4548, DC_TRANSPORT_SERIAL, NULL},
	{"Aeris",    "F11",                 DC_FAMILY_OCEANIC_ATOM2, 0x4549, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "OCi",                 DC_FAMILY_OCEANIC_ATOM2, 0x454B, DC_TRANSPORT_SERIAL, NULL},
	{"Aeris",    "A300CS",              DC_FAMILY_OCEANIC_ATOM2, 0x454C, DC_TRANSPORT_SERIAL, NULL},
	{"Beuchat",  "Mundial 3",           DC_FAMILY_OCEANIC_ATOM2, 0x4550, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "Pro Plus X",          DC_FAMILY_OCEANIC_ATOM2, 0x4552, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "F10",                 DC_FAMILY_OCEANIC_ATOM2, 0x4553, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "F11",                 DC_FAMILY_OCEANIC_ATOM2, 0x4554, DC_TRANSPORT_SERIAL, NULL},
	{"Subgear",  "XP-Air",              DC_FAMILY_OCEANIC_ATOM2, 0x4555, DC_TRANSPORT_SERIAL, NULL},
	{"Sherwood", "Vision",              DC_FAMILY_OCEANIC_ATOM2, 0x4556, DC_TRANSPORT_SERIAL, NULL},
	{"Oceanic",  "VTX",                 DC_FAMILY_OCEANIC_ATOM2, 0x4557, DC_TRANSPORT_SERIAL, NULL},
	{"Aqualung", "i300",                DC_FAMILY_OCEANIC_ATOM2, 0x4559, DC_TRANSPORT_SERIAL, NULL},
	{"Aqualung", "i750TC",              DC_FAMILY_OCEANIC_ATOM2, 0x455A, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLUETOOTH, NULL},
	{"Aqualung", "i450T",               DC_FAMILY_OCEANIC_ATOM2, 0x4641, DC_TRANSPORT_SERIAL, NULL},
	{"Aqualung", "i550",                DC_FAMILY_OCEANIC_ATOM2, 0x4642, DC_TRANSPORT_SERIAL, NULL},
	{"Aqualung", "i200",                DC_FAMILY_OCEANIC_ATOM2, 0x4646, DC_TRANSPORT_SERIAL, NULL},
	/* Mares Nemo */
	{"Mares", "Nemo",         DC_FAMILY_MARES_NEMO, 0, DC_TRANSPORT_SERIAL, NULL},
	{"Mares", "Nemo Steel",   DC_FAMILY_MARES_NEMO, 0, DC_TRANSPORT_SERIAL, NULL},
	{"Mares", "Nemo Titanium",DC_FAMILY_MARES_NEMO, 0, DC_TRANSPORT_SERIAL, NULL},
	{"Mares", "Nemo Excel",   DC_FAMILY_MARES_NEMO, 17, DC_TRANSPORT_SERIAL, NULL},
	{"Mares", "Nemo Apneist", DC_FAMILY_MARES_NEMO, 18, DC_TRANSPORT_SERIAL, NULL},
	/* Mares Puck */
	{"Mares", "Puck",      DC_FAMILY_MARES_PUCK, 7, DC_TRANSPORT_SERIAL, NULL},
	{"Mares", "Puck Air",  DC_FAMILY_MARES_PUCK, 19, DC_TRANSPORT_SERIAL, NULL},
	{"Mares", "Nemo Air",  DC_FAMILY_MARES_PUCK, 4, DC_TRANSPORT_SERIAL, NULL},
	{"Mares", "Nemo Wide", DC_FAMILY_MARES_PUCK, 1, DC_TRANSPORT_SERIAL, NULL},
	/* Mares Darwin */
	{"Mares", "Darwin",     DC_FAMILY_MARES_DARWIN , 0, DC_TRANSPORT_SERIAL, NULL},
	{"Mares", "M1",         DC_FAMILY_MARES_DARWIN , 0, DC_TRANSPORT_SERIAL, NULL},
	{"Mares", "M2",         DC_FAMILY_MARES_DARWIN , 0, DC_TRANSPORT_SERIAL, NULL},
	{"Mares", "Darwin Air", DC_FAMILY_MARES_DARWIN , 1, DC_TRANSPORT_SERIAL, NULL},
	{"Mares", "Airlab",     DC_FAMILY_MARES_DARWIN , 1, DC_TRANSPORT_SERIAL, NULL},
	/* Mares Icon HD */
	{"Mares", "Matrix",            DC_FAMILY_MARES_ICONHD , 0x0F, DC_TRANSPORT_SERIAL, NULL},
	{"Mares", "Smart",             DC_FAMILY_MARES_ICONHD , 0x000010, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLE, NULL},
	{"Mares", "Smart Apnea",       DC_FAMILY_MARES_ICONHD , 0x010010, DC_TRANSPORT_SERIAL, NULL},
	{"Mares", "Icon HD",           DC_FAMILY_MARES_ICONHD , 0x14, DC_TRANSPORT_SERIAL, NULL},
	{"Mares", "Icon HD Net Ready", DC_FAMILY_MARES_ICONHD , 0x15, DC_TRANSPORT_SERIAL, NULL},
	{"Mares", "Puck Pro",          DC_FAMILY_MARES_ICONHD , 0x18, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLE, NULL},
	{"Mares", "Nemo Wide 2",       DC_FAMILY_MARES_ICONHD , 0x19, DC_TRANSPORT_SERIAL, NULL},
	{"Mares", "Puck 2",            DC_FAMILY_MARES_ICONHD , 0x1F, DC_TRANSPORT_SERIAL, NULL},
	{"Mares", "Quad Air",          DC_FAMILY_MARES_ICONHD , 0x23, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLE, NULL},
	{"Mares", "Smart Air",         DC_FAMILY_MARES_ICONHD , 0x24, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLE, NULL},
	{"Mares", "Quad",              DC_FAMILY_MARES_ICONHD , 0x29, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLE, NULL},
	/* Heinrichs Weikamp */
	{"Heinrichs Weikamp", "OSTC",     DC_FAMILY_HW_OSTC, 0, DC_TRANSPORT_SERIAL, NULL},
	{"Heinrichs Weikamp", "OSTC Mk2", DC_FAMILY_HW_OSTC, 1, DC_TRANSPORT_SERIAL, NULL},
	{"Heinrichs Weikamp", "OSTC 2N",  DC_FAMILY_HW_OSTC, 2, DC_TRANSPORT_SERIAL, NULL},
	{"Heinrichs Weikamp", "OSTC 2C",  DC_FAMILY_HW_OSTC, 3, DC_TRANSPORT_SERIAL, NULL},
	{"Heinrichs Weikamp", "Frog",     DC_FAMILY_HW_FROG, 0, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLUETOOTH, dc_filter_hw},
	{"Heinrichs Weikamp", "OSTC 2",     DC_FAMILY_HW_OSTC3, 0x11, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLUETOOTH | DC_TRANSPORT_BLE, dc_filter_hw},
	{"Heinrichs Weikamp", "OSTC 2",     DC_FAMILY_HW_OSTC3, 0x13, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLUETOOTH | DC_TRANSPORT_BLE, dc_filter_hw},
	{"Heinrichs Weikamp", "OSTC 2",     DC_FAMILY_HW_OSTC3, 0x1B, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLUETOOTH | DC_TRANSPORT_BLE, dc_filter_hw},
	{"Heinrichs Weikamp", "OSTC 3",     DC_FAMILY_HW_OSTC3, 0x0A, DC_TRANSPORT_SERIAL, NULL},
	{"Heinrichs Weikamp", "OSTC Plus",  DC_FAMILY_HW_OSTC3, 0x13, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLUETOOTH | DC_TRANSPORT_BLE, dc_filter_hw},
	{"Heinrichs Weikamp", "OSTC Plus",  DC_FAMILY_HW_OSTC3, 0x1A, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLUETOOTH | DC_TRANSPORT_BLE, dc_filter_hw},
	{"Heinrichs Weikamp", "OSTC 4",     DC_FAMILY_HW_OSTC3, 0x3B, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLUETOOTH | DC_TRANSPORT_BLE, dc_filter_hw},
	{"Heinrichs Weikamp", "OSTC cR",    DC_FAMILY_HW_OSTC3, 0x05, DC_TRANSPORT_SERIAL, NULL},
	{"Heinrichs Weikamp", "OSTC cR",    DC_FAMILY_HW_OSTC3, 0x07, DC_TRANSPORT_SERIAL, NULL},
	{"Heinrichs Weikamp", "OSTC Sport", DC_FAMILY_HW_OSTC3, 0x12, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLUETOOTH | DC_TRANSPORT_BLE, dc_filter_hw},
	{"Heinrichs Weikamp", "OSTC Sport", DC_FAMILY_HW_OSTC3, 0x13, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLUETOOTH | DC_TRANSPORT_BLE, dc_filter_hw},
	{"Heinrichs Weikamp", "OSTC 2 TR",  DC_FAMILY_HW_OSTC3, 0x33, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLUETOOTH | DC_TRANSPORT_BLE, dc_filter_hw},
	/* Cressi Edy */
	{"Tusa",   "IQ-700", DC_FAMILY_CRESSI_EDY, 0x05, DC_TRANSPORT_SERIAL, NULL},
	{"Cressi", "Edy",    DC_FAMILY_CRESSI_EDY, 0x08, DC_TRANSPORT_SERIAL, NULL},
	/* Cressi Leonardo */
	{"Cressi", "Leonardo", DC_FAMILY_CRESSI_LEONARDO, 1, DC_TRANSPORT_SERIAL, NULL},
	{"Cressi", "Giotto",   DC_FAMILY_CRESSI_LEONARDO, 4, DC_TRANSPORT_SERIAL, NULL},
	{"Cressi", "Newton",   DC_FAMILY_CRESSI_LEONARDO, 5, DC_TRANSPORT_SERIAL, NULL},
	{"Cressi", "Drake",    DC_FAMILY_CRESSI_LEONARDO, 6, DC_TRANSPORT_SERIAL, NULL},
	/* Zeagle N2iTiON3 */
	{"Zeagle",    "N2iTiON3",   DC_FAMILY_ZEAGLE_N2ITION3, 0, DC_TRANSPORT_SERIAL, NULL},
	{"Apeks",     "Quantum X",  DC_FAMILY_ZEAGLE_N2ITION3, 0, DC_TRANSPORT_SERIAL, NULL},
	{"Dive Rite", "NiTek Trio", DC_FAMILY_ZEAGLE_N2ITION3, 0, DC_TRANSPORT_SERIAL, NULL},
	{"Scubapro",  "XTender 5",  DC_FAMILY_ZEAGLE_N2ITION3, 0, DC_TRANSPORT_SERIAL, NULL},
	/* Atomic Aquatics Cobalt */
	{"Atomic Aquatics", "Cobalt", DC_FAMILY_ATOMICS_COBALT, 0, DC_TRANSPORT_USB, NULL},
	{"Atomic Aquatics", "Cobalt 2", DC_FAMILY_ATOMICS_COBALT, 2, DC_TRANSPORT_USB, NULL},
	/* Shearwater Predator */
	{"Shearwater", "Predator", DC_FAMILY_SHEARWATER_PREDATOR, 2, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLUETOOTH, dc_filter_shearwater},
	/* Shearwater Petrel */
	{"Shearwater", "Petrel",    DC_FAMILY_SHEARWATER_PETREL, 3, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLUETOOTH, dc_filter_shearwater},
	{"Shearwater", "Petrel 2",  DC_FAMILY_SHEARWATER_PETREL, 3, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLUETOOTH | DC_TRANSPORT_BLE, dc_filter_shearwater},
	{"Shearwater", "Nerd",      DC_FAMILY_SHEARWATER_PETREL, 4, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLUETOOTH, dc_filter_shearwater},
	{"Shearwater", "Perdix",    DC_FAMILY_SHEARWATER_PETREL, 5, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLUETOOTH | DC_TRANSPORT_BLE, dc_filter_shearwater},
	{"Shearwater", "Perdix AI", DC_FAMILY_SHEARWATER_PETREL, 6, DC_TRANSPORT_BLE, dc_filter_shearwater},
	{"Shearwater", "Nerd 2",    DC_FAMILY_SHEARWATER_PETREL, 7, DC_TRANSPORT_BLE, dc_filter_shearwater},
	/* Dive Rite NiTek Q */
	{"Dive Rite", "NiTek Q",   DC_FAMILY_DIVERITE_NITEKQ, 0, DC_TRANSPORT_SERIAL, NULL},
	/* Citizen Hyper Aqualand */
	{"Citizen", "Hyper Aqualand", DC_FAMILY_CITIZEN_AQUALAND, 0, DC_TRANSPORT_SERIAL, NULL},
	/* DiveSystem/Ratio iDive */
	{"DiveSystem", "Orca",          DC_FAMILY_DIVESYSTEM_IDIVE, 0x02, DC_TRANSPORT_SERIAL, NULL},
	{"DiveSystem", "iDive Pro",     DC_FAMILY_DIVESYSTEM_IDIVE, 0x03, DC_TRANSPORT_SERIAL, NULL},
	{"DiveSystem", "iDive DAN",     DC_FAMILY_DIVESYSTEM_IDIVE, 0x04, DC_TRANSPORT_SERIAL, NULL},
	{"DiveSystem", "iDive Tech",    DC_FAMILY_DIVESYSTEM_IDIVE, 0x05, DC_TRANSPORT_SERIAL, NULL},
	{"DiveSystem", "iDive Reb",     DC_FAMILY_DIVESYSTEM_IDIVE, 0x06, DC_TRANSPORT_SERIAL, NULL},
	{"DiveSystem", "iDive Stealth", DC_FAMILY_DIVESYSTEM_IDIVE, 0x07, DC_TRANSPORT_SERIAL, NULL},
	{"DiveSystem", "iDive Free",    DC_FAMILY_DIVESYSTEM_IDIVE, 0x08, DC_TRANSPORT_SERIAL, NULL},
	{"DiveSystem", "iDive Easy",    DC_FAMILY_DIVESYSTEM_IDIVE, 0x09, DC_TRANSPORT_SERIAL, NULL},
	{"DiveSystem", "iDive X3M",     DC_FAMILY_DIVESYSTEM_IDIVE, 0x0A, DC_TRANSPORT_SERIAL, NULL},
	{"DiveSystem", "iDive Deep",    DC_FAMILY_DIVESYSTEM_IDIVE, 0x0B, DC_TRANSPORT_SERIAL, NULL},
	{"Ratio",      "iX3M Easy",     DC_FAMILY_DIVESYSTEM_IDIVE, 0x22, DC_TRANSPORT_SERIAL, NULL},
	{"Ratio",      "iX3M Deep",     DC_FAMILY_DIVESYSTEM_IDIVE, 0x23, DC_TRANSPORT_SERIAL, NULL},
	{"Ratio",      "iX3M Tech+",    DC_FAMILY_DIVESYSTEM_IDIVE, 0x24, DC_TRANSPORT_SERIAL, NULL},
	{"Ratio",      "iX3M Reb",      DC_FAMILY_DIVESYSTEM_IDIVE, 0x25, DC_TRANSPORT_SERIAL, NULL},
	{"Ratio",      "iX3M Pro Easy", DC_FAMILY_DIVESYSTEM_IDIVE, 0x32, DC_TRANSPORT_SERIAL, NULL},
	{"Ratio",      "iX3M Pro Deep", DC_FAMILY_DIVESYSTEM_IDIVE, 0x34, DC_TRANSPORT_SERIAL, NULL},
	{"Ratio",      "iX3M Pro Tech+",DC_FAMILY_DIVESYSTEM_IDIVE, 0x35, DC_TRANSPORT_SERIAL, NULL},
	{"Ratio",      "iDive Free",    DC_FAMILY_DIVESYSTEM_IDIVE, 0x40, DC_TRANSPORT_SERIAL, NULL},
	{"Ratio",      "iDive Easy",    DC_FAMILY_DIVESYSTEM_IDIVE, 0x42, DC_TRANSPORT_SERIAL, NULL},
	{"Ratio",      "iDive Deep",    DC_FAMILY_DIVESYSTEM_IDIVE, 0x44, DC_TRANSPORT_SERIAL, NULL},
	{"Ratio",      "iDive Tech+",   DC_FAMILY_DIVESYSTEM_IDIVE, 0x45, DC_TRANSPORT_SERIAL, NULL},
	{"Seac",       "Jack",          DC_FAMILY_DIVESYSTEM_IDIVE, 0x1000, DC_TRANSPORT_SERIAL, NULL},
	{"Seac",       "Guru",          DC_FAMILY_DIVESYSTEM_IDIVE, 0x1002, DC_TRANSPORT_SERIAL, NULL},
	/* Cochran Commander */
	{"Cochran", "Commander TM", DC_FAMILY_COCHRAN_COMMANDER, 0, DC_TRANSPORT_SERIAL, NULL},
	{"Cochran", "Commander I",  DC_FAMILY_COCHRAN_COMMANDER, 1, DC_TRANSPORT_SERIAL, NULL},
	{"Cochran", "Commander II", DC_FAMILY_COCHRAN_COMMANDER, 2, DC_TRANSPORT_SERIAL, NULL},
	{"Cochran", "EMC-14",       DC_FAMILY_COCHRAN_COMMANDER, 3, DC_TRANSPORT_SERIAL, NULL},
	{"Cochran", "EMC-16",       DC_FAMILY_COCHRAN_COMMANDER, 4, DC_TRANSPORT_SERIAL, NULL},
	{"Cochran", "EMC-20H",      DC_FAMILY_COCHRAN_COMMANDER, 5, DC_TRANSPORT_SERIAL, NULL},
	/* Tecdiving DiveComputer.eu */
	{"Tecdiving", "DiveComputer.eu", DC_FAMILY_TECDIVING_DIVECOMPUTEREU, 0, DC_TRANSPORT_SERIAL | DC_TRANSPORT_BLUETOOTH, dc_filter_tecdiving},
	/* Garmin */
	{"Garmin", "Descent Mk1", DC_FAMILY_GARMIN, 2859, DC_TRANSPORT_USBSTORAGE, dc_filter_garmin},
};

static int
dc_filter_internal_name (const char *name, const char *values[], size_t count)
{
	if (name == NULL)
		return 0;

	for (size_t i = 0; i < count; ++i) {
		if (strcasecmp (name, values[i]) == 0) {
			return 1;
		}
	}

	return 0;
}

static int
dc_filter_internal_usb (const dc_usb_desc_t *desc, const dc_usb_desc_t values[], size_t count)
{
	if (desc == NULL)
		return 0;

	for (size_t i = 0; i < count; ++i) {
		if (desc->vid == values[i].vid &&
			desc->pid == values[i].pid) {
			return 1;
		}
	}

	return 0;
}

static int
dc_filter_internal_rfcomm (const char *name)
{
	static const char *prefixes[] = {
#if defined (__linux__)
		"/dev/rfcomm",
#endif
		NULL
	};

	if (name == NULL)
		return 0;

	for (size_t i = 0; prefixes[i] != NULL; ++i) {
		if (strncmp (name, prefixes[i], strlen (prefixes[i])) == 0)
			return 1;
	}

	return prefixes[0] == NULL;
}

static int dc_filter_uwatec (dc_transport_t transport, const void *userdata)
{
	static const char *irda[] = {
		"Aladin Smart Com",
		"Aladin Smart Pro",
		"Aladin Smart Tec",
		"Aladin Smart Z",
		"Uwatec Aladin",
		"UWATEC Galileo",
		"UWATEC Galileo Sol",
	};
	static const dc_usb_desc_t usbhid[] = {
		{0x2e6c, 0x3201}, // G2
		{0x2e6c, 0x3211}, // G2 Console
		{0xc251, 0x2006}, // Aladin Square
	};

	if (transport == DC_TRANSPORT_IRDA) {
		return dc_filter_internal_name ((const char *) userdata, irda, C_ARRAY_SIZE(irda));
	} else if (transport == DC_TRANSPORT_USBHID) {
		return dc_filter_internal_usb ((const dc_usb_desc_t *) userdata, usbhid, C_ARRAY_SIZE(usbhid));
	}

	return 1;
}

static int dc_filter_suunto (dc_transport_t transport, const void *userdata)
{
	static const dc_usb_desc_t usbhid[] = {
		{0x1493, 0x0030}, // Eon Steel
		{0x1493, 0x0033}, // Eon Core
	};

	if (transport == DC_TRANSPORT_USBHID) {
		return dc_filter_internal_usb ((const dc_usb_desc_t *) userdata, usbhid, C_ARRAY_SIZE(usbhid));
	}

	return 1;
}

static int dc_filter_hw (dc_transport_t transport, const void *userdata)
{
	if (transport == DC_TRANSPORT_BLUETOOTH) {
		return strncasecmp ((const char *) userdata, "OSTC", 4) == 0 ||
			strncasecmp ((const char *) userdata, "FROG", 4) == 0;
	} else if (transport == DC_TRANSPORT_SERIAL) {
		return dc_filter_internal_rfcomm ((const char *) userdata);
	}

	return 1;
}

static int dc_filter_shearwater (dc_transport_t transport, const void *userdata)
{
	static const char *bluetooth[] = {
		"Predator",
		"Petrel",
		"Nerd",
		"Perdix",
	};

	if (transport == DC_TRANSPORT_BLUETOOTH) {
		return dc_filter_internal_name ((const char *) userdata, bluetooth, C_ARRAY_SIZE(bluetooth));
	} else if (transport == DC_TRANSPORT_SERIAL) {
		return dc_filter_internal_rfcomm ((const char *) userdata);
	}

	return 1;
}

static int dc_filter_tecdiving (dc_transport_t transport, const void *userdata)
{
	static const char *bluetooth[] = {
		"DiveComputer",
	};

	if (transport == DC_TRANSPORT_BLUETOOTH) {
		return dc_filter_internal_name ((const char *) userdata, bluetooth, C_ARRAY_SIZE(bluetooth));
	} else if (transport == DC_TRANSPORT_SERIAL) {
		return dc_filter_internal_rfcomm ((const char *) userdata);
	}

	return 1;
}

static int dc_filter_garmin (dc_transport_t transport, const void *userdata)
{
	static const dc_usb_desc_t usbhid[] = {
		{0x091e, 0x2b2b}, // Garmin Descent Mk1
	};

	if (transport == DC_TRANSPORT_USBSTORAGE) {
		return dc_filter_internal_usb ((const dc_usb_desc_t *) userdata, usbhid, C_ARRAY_SIZE(usbhid));
	}

	return 1;
}

dc_status_t
dc_descriptor_iterator (dc_iterator_t **out)
{
	dc_descriptor_iterator_t *iterator = NULL;

	if (out == NULL)
		return DC_STATUS_INVALIDARGS;

	iterator = (dc_descriptor_iterator_t *) dc_iterator_allocate (NULL, &dc_descriptor_iterator_vtable);
	if (iterator == NULL)
		return DC_STATUS_NOMEMORY;

	iterator->current = 0;

	*out = (dc_iterator_t *) iterator;

	return DC_STATUS_SUCCESS;
}

static dc_status_t
dc_descriptor_iterator_next (dc_iterator_t *abstract, void *out)
{
	dc_descriptor_iterator_t *iterator = (dc_descriptor_iterator_t *) abstract;
	dc_descriptor_t **item = (dc_descriptor_t **) out;

	if (iterator->current >= C_ARRAY_SIZE (g_descriptors))
		return DC_STATUS_DONE;

	/*
	 * The explicit cast from a const to a non-const pointer is safe here. The
	 * public interface doesn't support write access, and therefore descriptor
	 * objects are always read-only. However, the cast allows to return a direct
	 * reference to the entries in the table, avoiding the overhead of
	 * allocating (and freeing) memory for a deep copy.
	 */
	*item = (dc_descriptor_t *) &g_descriptors[iterator->current++];

	return DC_STATUS_SUCCESS;
}

void
dc_descriptor_free (dc_descriptor_t *descriptor)
{
	return;
}

const char *
dc_descriptor_get_vendor (dc_descriptor_t *descriptor)
{
	if (descriptor == NULL)
		return NULL;

	return descriptor->vendor;
}

const char *
dc_descriptor_get_product (dc_descriptor_t *descriptor)
{
	if (descriptor == NULL)
		return NULL;

	return descriptor->product;
}

dc_family_t
dc_descriptor_get_type (dc_descriptor_t *descriptor)
{
	if (descriptor == NULL)
		return DC_FAMILY_NULL;

	return descriptor->type;
}

unsigned int
dc_descriptor_get_model (dc_descriptor_t *descriptor)
{
	if (descriptor == NULL)
		return 0;

	return descriptor->model;
}

unsigned int
dc_descriptor_get_transports (dc_descriptor_t *descriptor)
{
	if (descriptor == NULL)
		return DC_TRANSPORT_NONE;

	return descriptor->transports;
}

dc_filter_t
dc_descriptor_get_filter (dc_descriptor_t *descriptor)
{
	if (descriptor == NULL)
		return NULL;

	return descriptor->filter;
}
