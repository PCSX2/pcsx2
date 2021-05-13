/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"

#include <stdlib.h>
#include <arpa/inet.h>

#include "DEV9/DEV9.h"
#include "AppConfig.h"

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>

#include <libxml/parser.h>
#include <libxml/tree.h>

void SaveConf()
{

	xmlDocPtr doc = NULL; /* document pointer */
	xmlNodePtr root_node = NULL;
	char buff[256];

	/*
     * Creates a new document, a node and set it as a root node
     */
	doc = xmlNewDoc(BAD_CAST "1.0");
	root_node = xmlNewNode(NULL, BAD_CAST "dev9");
	xmlDocSetRootElement(doc, root_node);

	xmlNewChild(root_node, NULL, BAD_CAST "Eth",
				BAD_CAST config.Eth);

	sprintf(buff, "%d", (int)config.EthApi);
	xmlNewChild(root_node, NULL, BAD_CAST "EthApi",
				BAD_CAST buff);

	sprintf(buff, "%d", config.InterceptDHCP);
	xmlNewChild(root_node, NULL, BAD_CAST "InterceptDHCP",
				BAD_CAST buff);

	inet_ntop(AF_INET, &config.PS2IP, buff, 256);
	xmlNewChild(root_node, NULL, BAD_CAST "PS2IP",
				BAD_CAST buff);

	inet_ntop(AF_INET, &config.Mask, buff, 256);
	xmlNewChild(root_node, NULL, BAD_CAST "Subnet",
				BAD_CAST buff);

	sprintf(buff, "%d", config.AutoMask);
	xmlNewChild(root_node, NULL, BAD_CAST "AutoSubnet",
				BAD_CAST buff);

	inet_ntop(AF_INET, &config.Gateway, buff, 256);
	xmlNewChild(root_node, NULL, BAD_CAST "Gateway",
				BAD_CAST buff);

	sprintf(buff, "%d", config.AutoGateway);
	xmlNewChild(root_node, NULL, BAD_CAST "AutoGateway",
				BAD_CAST buff);

	inet_ntop(AF_INET, &config.DNS1, buff, 256);
	xmlNewChild(root_node, NULL, BAD_CAST "DNS1",
				BAD_CAST buff);

	sprintf(buff, "%d", config.AutoDNS1);
	xmlNewChild(root_node, NULL, BAD_CAST "AutoDNS1",
				BAD_CAST buff);

	inet_ntop(AF_INET, &config.DNS2, buff, 256);
	xmlNewChild(root_node, NULL, BAD_CAST "DNS2",
				BAD_CAST buff);

	sprintf(buff, "%d", config.AutoDNS2);
	xmlNewChild(root_node, NULL, BAD_CAST "AutoDNS2",
				BAD_CAST buff);

	xmlNewChild(root_node, NULL, BAD_CAST "Hdd",
				BAD_CAST config.Hdd);

	sprintf(buff, "%d", config.HddSize);
	xmlNewChild(root_node, NULL, BAD_CAST "HddSize",
				BAD_CAST buff);

	sprintf(buff, "%d", config.ethEnable);
	xmlNewChild(root_node, NULL, BAD_CAST "ethEnable",
				BAD_CAST buff);

	sprintf(buff, "%d", config.hddEnable);
	xmlNewChild(root_node, NULL, BAD_CAST "hddEnable",
				BAD_CAST buff);
	/*
     * Dumping document to stdio or file
     */


	const std::string file(GetSettingsFolder().Combine(wxString("DEV9.cfg")).GetFullPath());

	Console.WriteLn("DEV9: CONF: %s", file.c_str());

	xmlSaveFormatFileEnc(file.c_str(), doc, "UTF-8", 1);
	//    free(configFile);

	/*free the document */
	xmlFreeDoc(doc);

	/*
     *Free the global variables that may
     *have been allocated by the parser.
     */
	xmlCleanupParser();
}

void LoadConf()
{

	const std::string file(GetSettingsFolder().Combine(wxString("DEV9.cfg")).GetFullPath());
	if (-1 == access(file.c_str(), F_OK))
		return;

	memset(&config, 0, sizeof(config));
	config.EthApi = NetApi::PCAP_Switched;

	// Read the files
	xmlDoc* doc = NULL;
	xmlNode* cur_node = NULL;

	doc = xmlReadFile(file.c_str(), NULL, 0);

	if (doc == NULL)
	{
		Console.Error("Unable to parse configuration file! Suggest deleting it and starting over.");
	}

	for (cur_node = xmlDocGetRootElement(doc)->children; cur_node; cur_node = cur_node->next)
	{
		if (cur_node->type == XML_ELEMENT_NODE)
		{
			//            printf("node type: Element, name: %s\n", cur_node->name);
			if (0 == strcmp((const char*)cur_node->name, "Eth"))
			{
				strcpy(config.Eth, (const char*)xmlNodeGetContent(cur_node));
			}
			if (0 == strcmp((const char*)cur_node->name, "EthApi"))
			{
				config.EthApi = (NetApi)atoi((const char*)xmlNodeGetContent(cur_node));
			}
			if (0 == strcmp((const char*)cur_node->name, "InterceptDHCP"))
			{
				config.InterceptDHCP = atoi((const char*)xmlNodeGetContent(cur_node));
			}
			if (0 == strcmp((const char*)cur_node->name, "PS2IP"))
			{
				inet_pton(AF_INET, (const char*)xmlNodeGetContent(cur_node), &config.PS2IP);
			}
			if (0 == strcmp((const char*)cur_node->name, "Subnet"))
			{
				inet_pton(AF_INET, (const char*)xmlNodeGetContent(cur_node), &config.Mask);
			}
			if (0 == strcmp((const char*)cur_node->name, "AutoSubnet"))
			{
				config.AutoMask = atoi((const char*)xmlNodeGetContent(cur_node));
			}
			if (0 == strcmp((const char*)cur_node->name, "Gateway"))
			{
				inet_pton(AF_INET, (const char*)xmlNodeGetContent(cur_node), &config.Gateway);
			}
			if (0 == strcmp((const char*)cur_node->name, "AutoGateway"))
			{
				config.AutoGateway = atoi((const char*)xmlNodeGetContent(cur_node));
			}
			if (0 == strcmp((const char*)cur_node->name, "DNS1"))
			{
				inet_pton(AF_INET, (const char*)xmlNodeGetContent(cur_node), &config.DNS1);
			}
			if (0 == strcmp((const char*)cur_node->name, "AutoDNS1"))
			{
				config.AutoDNS1 = atoi((const char*)xmlNodeGetContent(cur_node));
			}
			if (0 == strcmp((const char*)cur_node->name, "DNS2"))
			{
				inet_pton(AF_INET, (const char*)xmlNodeGetContent(cur_node), &config.DNS2);
			}
			if (0 == strcmp((const char*)cur_node->name, "AutoDNS2"))
			{
				config.AutoDNS2 = atoi((const char*)xmlNodeGetContent(cur_node));
			}
			if (0 == strcmp((const char*)cur_node->name, "Hdd"))
			{
				strcpy(config.Hdd, (const char*)xmlNodeGetContent(cur_node));
			}
			if (0 == strcmp((const char*)cur_node->name, "HddSize"))
			{
				config.HddSize = atoi((const char*)xmlNodeGetContent(cur_node));
			}
			if (0 == strcmp((const char*)cur_node->name, "ethEnable"))
			{
				config.ethEnable = atoi((const char*)xmlNodeGetContent(cur_node));
			}
			if (0 == strcmp((const char*)cur_node->name, "hddEnable"))
			{
				config.hddEnable = atoi((const char*)xmlNodeGetContent(cur_node));
			}
		}
	}

	//    free(configFile);
	xmlFreeDoc(doc);
	xmlCleanupParser();
}
