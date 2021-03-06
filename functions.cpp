/*
 * License: MIT
 *
 * Copyright (c) 2012-2017 James Bensley.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 * 
 * File: Etherate Gerneral Functions
 *
 * File Contents:
 * broadcast_etherate()
 * build_headers()
 * build_tlv()
 * build_sub_tlv()
 * cli_args()
 * explode_char()
 * get_interface_mtu_by_name()
 * get_sock_interface()
 * htonll()
 * list_interfaces()
 * ntohll()
 * print_usage()
 * remove_interface_promisc()
 * reset_app()
 * set_interface_promisc()
 * set_sock_interface_index()
 * set_sock_interface_name()
 * signal_handler()
 * sync_settings()
 *
 */



#include "functions.h"


int16_t broadcast_etherate(struct FRAME_HEADERS *FRAME_HEADERS,
                           struct TEST_INTERFACE *TEST_INTERFACE)
{

    uint8_t TEMP_MAC[6] = {
    FRAME_HEADERS->DEST_MAC[0],
    FRAME_HEADERS->DEST_MAC[1],
    FRAME_HEADERS->DEST_MAC[2],
    FRAME_HEADERS->DEST_MAC[3],
    FRAME_HEADERS->DEST_MAC[4],
    FRAME_HEADERS->DEST_MAC[5],
    };

    FRAME_HEADERS->DEST_MAC[0] = 0xFF;
    FRAME_HEADERS->DEST_MAC[1] = 0xFF;
    FRAME_HEADERS->DEST_MAC[2] = 0xFF;
    FRAME_HEADERS->DEST_MAC[3] = 0xFF;
    FRAME_HEADERS->DEST_MAC[4] = 0xFF;
    FRAME_HEADERS->DEST_MAC[5] = 0xFF;

    // Rebuild frame headers with destination MAC
    build_headers(FRAME_HEADERS);

    build_tlv(FRAME_HEADERS, htons(TYPE_BROADCAST), htonl(VALUE_PRESENCE));

    // Build a dummy sub-TLV to align the buffers and pointers
    build_sub_tlv(FRAME_HEADERS, htons(TYPE_APPLICATION_SUB_TLV),
                  htonll(VALUE_DUMMY));

    int16_t TX_RET_VAL = 0;

    printf("Sending gratuitous broadcasts...\n");
    for (uint8_t i=1; i<=3; i += 1)
    {

        TX_RET_VAL = sendto(TEST_INTERFACE->SOCKET_FD, FRAME_HEADERS->TX_BUFFER,
                            FRAME_HEADERS->LENGTH+FRAME_HEADERS->TLV_SIZE, 0, 
                            (struct sockaddr*)&TEST_INTERFACE->SOCKET_ADDRESS,
                            sizeof(TEST_INTERFACE->SOCKET_ADDRESS));

        if (TX_RET_VAL <= 0)
        {
            perror("Error: Broadcast failed ");
            return TX_RET_VAL;
        }

        sleep(1);

    }

    printf("Done.\n");


    // Restore the original destination MAC
    FRAME_HEADERS->DEST_MAC[0] = TEMP_MAC[0];
    FRAME_HEADERS->DEST_MAC[1] = TEMP_MAC[1];
    FRAME_HEADERS->DEST_MAC[2] = TEMP_MAC[2];
    FRAME_HEADERS->DEST_MAC[3] = TEMP_MAC[3];
    FRAME_HEADERS->DEST_MAC[4] = TEMP_MAC[4];
    FRAME_HEADERS->DEST_MAC[5] = TEMP_MAC[5];

    return EXIT_SUCCESS;

}



void build_headers(struct FRAME_HEADERS *FRAME_HEADERS)
{

    uint16_t MPLS_OFFSET     = 0;
    uint16_t ETHERNET_OFFSET = 0;
    uint32_t MPLS_HEADER     = 0;
    uint16_t TPI             = 0;
    uint16_t TCI             = 0;
    uint16_t *pTPI           = &TPI;
    uint16_t *pTCI           = &TCI;
    uint16_t VLAN_ID_TMP;


    // Check if transporting over MPLS
    if (FRAME_HEADERS->MPLS_LABELS > 0) {

        // Copy the destination and source LSR MAC addresses
        memcpy((void*)FRAME_HEADERS->TX_BUFFER,
               (void*)FRAME_HEADERS->LSP_DEST_MAC, ETH_ALEN);

        MPLS_OFFSET+=ETH_ALEN;

        memcpy((void*)(FRAME_HEADERS->TX_BUFFER+MPLS_OFFSET),
               (void*)FRAME_HEADERS->LSP_SOURCE_MAC, ETH_ALEN);

        MPLS_OFFSET+=ETH_ALEN;

        // Push on the ethertype for unicast MPLS
        TPI = htons(0x8847);
        memcpy((void*)(FRAME_HEADERS->TX_BUFFER+MPLS_OFFSET), pTPI, sizeof(TPI));
        MPLS_OFFSET+=sizeof(TPI);

        // For each MPLS label copy it onto the stack
        uint8_t i = 0;
        while (i < FRAME_HEADERS->MPLS_LABELS) {

            MPLS_HEADER = (FRAME_HEADERS->MPLS_LABEL[i] & 0xFFFFF) << 12;
            MPLS_HEADER = MPLS_HEADER | (FRAME_HEADERS->MPLS_EXP[i] & 0x07) << 9;

            // BoS bit
            if (i == FRAME_HEADERS->MPLS_LABELS - 1) {
                MPLS_HEADER = MPLS_HEADER | 1 << 8;
            } else {
                MPLS_HEADER = MPLS_HEADER | 0 << 8;
            }

            MPLS_HEADER = MPLS_HEADER | (FRAME_HEADERS->MPLS_TTL[i] & 0xFF);

            MPLS_HEADER = htonl(MPLS_HEADER);

            memcpy((void*)(FRAME_HEADERS->TX_BUFFER+MPLS_OFFSET),
                   &MPLS_HEADER, sizeof(MPLS_HEADER));

            MPLS_OFFSET+=sizeof(MPLS_HEADER);

            MPLS_HEADER = 0;

            i += 1;
        }

        // Check if we need to push on a pseudowire control word
        if (FRAME_HEADERS->PWE_CONTROL_WORD == 1){
            memset((void*)(FRAME_HEADERS->TX_BUFFER+MPLS_OFFSET), 0, 4);
            MPLS_OFFSET+=4;
        }

    }

    ETHERNET_OFFSET+=MPLS_OFFSET;

    // Copy the destination and source MAC addresses
    memcpy((void*)(FRAME_HEADERS->TX_BUFFER+ETHERNET_OFFSET),
           (void*)FRAME_HEADERS->DEST_MAC, ETH_ALEN);

    ETHERNET_OFFSET+=ETH_ALEN;

    memcpy((void*)(FRAME_HEADERS->TX_BUFFER+ETHERNET_OFFSET),
           (void*)FRAME_HEADERS->SOURCE_MAC, ETH_ALEN);

    ETHERNET_OFFSET+=ETH_ALEN;

    // Check if QinQ VLAN ID has been supplied
    if (FRAME_HEADERS->QINQ_PCP != QINQ_PCP_DEF ||
        FRAME_HEADERS->QINQ_ID != QINQ_ID_DEF)
    {

        // Add on the QinQ Tag Protocol Identifier
        // 0x88a8 == IEEE802.1ad, 0x9100 == older IEEE802.1QinQ
        TPI = htons(0x88a8);
        memcpy((void*)(FRAME_HEADERS->TX_BUFFER+ETHERNET_OFFSET),
               pTPI, sizeof(TPI));

        ETHERNET_OFFSET+=sizeof(TPI);

        // Build the QinQ Tag Control Identifier...

        // PCP value
        VLAN_ID_TMP = FRAME_HEADERS->QINQ_ID;
        TCI = (FRAME_HEADERS->QINQ_PCP & 0x07) << 5;

        // DEI value
        if (FRAME_HEADERS->QINQ_DEI==1)
        {
            TCI = TCI | (1 << 4);
        }

        // VLAN ID, first 4 bits
        FRAME_HEADERS->QINQ_ID = FRAME_HEADERS->QINQ_ID >> 8;
        TCI = TCI | (FRAME_HEADERS->QINQ_ID & 0x0f);

        // VLAN ID, last 8 bits
        FRAME_HEADERS->QINQ_ID = VLAN_ID_TMP;
        FRAME_HEADERS->QINQ_ID = FRAME_HEADERS->QINQ_ID << 8;
        TCI = TCI | (FRAME_HEADERS->QINQ_ID & 0xffff);
        FRAME_HEADERS->QINQ_ID = VLAN_ID_TMP;

        memcpy((void*)(FRAME_HEADERS->TX_BUFFER+ETHERNET_OFFSET),
               pTCI, sizeof(TCI));

        ETHERNET_OFFSET+=sizeof(TCI);

        // If an outer VLAN ID has been set, but not an inner one
        // (assume to be a mistake) set it to 1 so the frame is still valid
        if (FRAME_HEADERS->VLAN_ID == 0) FRAME_HEADERS->VLAN_ID = 1;

    }

    // Check to see if an inner VLAN PCP value or VLAN ID has been supplied
    if (FRAME_HEADERS->PCP != PCP_DEF || FRAME_HEADERS->VLAN_ID != VLAN_ID_DEF)
    {

        TPI = htons(0x8100);
        memcpy((void*)(FRAME_HEADERS->TX_BUFFER+ETHERNET_OFFSET),
               pTPI, sizeof(TPI));

        ETHERNET_OFFSET+=sizeof(TPI);


        // Build the inner VLAN TCI...

        // PCP value
        VLAN_ID_TMP = FRAME_HEADERS->VLAN_ID;
        TCI = (FRAME_HEADERS->PCP & 0x07) << 5;

        // DEI value
        if (FRAME_HEADERS->VLAN_DEI==1)
        {
            TCI = TCI | (1 << 4);
        }

        // VLAN ID, first 4 bits
        FRAME_HEADERS->VLAN_ID = FRAME_HEADERS->VLAN_ID >> 8;
        TCI = TCI | (FRAME_HEADERS->VLAN_ID & 0x0f);

        // VLAN ID, last 8 bits
        FRAME_HEADERS->VLAN_ID = VLAN_ID_TMP;
        FRAME_HEADERS->VLAN_ID = FRAME_HEADERS->VLAN_ID << 8;
        TCI = TCI | (FRAME_HEADERS->VLAN_ID & 0xffff);
        FRAME_HEADERS->VLAN_ID = VLAN_ID_TMP;

        memcpy((void*)(FRAME_HEADERS->TX_BUFFER+ETHERNET_OFFSET),
               pTCI, sizeof(TCI));

        ETHERNET_OFFSET+=sizeof(TCI);

    }

    // Push on the ethertype for the Etherate payload
    TPI = htons(FRAME_HEADERS->ETHERTYPE);

    memcpy((void*)(FRAME_HEADERS->TX_BUFFER+ETHERNET_OFFSET),
           pTPI, sizeof(TPI));

    ETHERNET_OFFSET+=sizeof(TPI);

    FRAME_HEADERS->LENGTH = ETHERNET_OFFSET;


    // Pointers to the payload section of the frame
    FRAME_HEADERS->TX_DATA = FRAME_HEADERS->TX_BUFFER + FRAME_HEADERS->LENGTH;

    FRAME_HEADERS->RX_DATA = FRAME_HEADERS->RX_BUFFER + FRAME_HEADERS->LENGTH;

    /* When receiving a VLAN tagged frame (one or more VLAN tags) the Linux
     * Kernel is stripping off the outer most VLAN, so for the RX buffer the
     * data starts 4 bytes earlier, see this post:
     * http://stackoverflow.com/questions/24355597/linux-when-sending-ethernet-
     * frames-the-ethertype-is-being-re-written
     */
    if (FRAME_HEADERS->VLAN_ID != VLAN_ID_DEF ||
        FRAME_HEADERS->QINQ_ID != QINQ_ID_DEF)
    {
        FRAME_HEADERS->RX_DATA -=4;
    }

}



void build_tlv(struct FRAME_HEADERS *FRAME_HEADERS, uint16_t TLV_TYPE,
               uint32_t TLV_VALUE)
{

    uint8_t TLV_LENGTH = sizeof(TLV_VALUE);

    uint8_t *BUFFER_OFFSET = FRAME_HEADERS->TX_DATA;
    memcpy(BUFFER_OFFSET, &TLV_TYPE, sizeof(TLV_TYPE));

    BUFFER_OFFSET += sizeof(TLV_TYPE);
    memcpy(BUFFER_OFFSET, &TLV_LENGTH, sizeof(TLV_LENGTH));
    
    BUFFER_OFFSET += sizeof(TLV_LENGTH);
    memcpy(BUFFER_OFFSET, &TLV_VALUE, sizeof(TLV_VALUE));

    FRAME_HEADERS->RX_TLV_TYPE  = (uint16_t*)FRAME_HEADERS->RX_DATA;

    FRAME_HEADERS->RX_TLV_VALUE = (uint32_t*)(FRAME_HEADERS->RX_DATA +
                                  sizeof(TLV_TYPE) + sizeof(TLV_LENGTH));

}



void build_sub_tlv(struct FRAME_HEADERS *FRAME_HEADERS, uint16_t SUB_TLV_TYPE,
                   uint64_t SUB_TLV_VALUE)
{

    uint8_t SUB_TLV_LENGTH = sizeof(SUB_TLV_VALUE);

    uint8_t *BUFFER_OFFSET = FRAME_HEADERS->TX_DATA + FRAME_HEADERS->TLV_SIZE;
    memcpy(BUFFER_OFFSET, &SUB_TLV_TYPE, sizeof(SUB_TLV_TYPE));
    
    BUFFER_OFFSET += sizeof(SUB_TLV_TYPE);
    memcpy(BUFFER_OFFSET, &SUB_TLV_LENGTH, sizeof(SUB_TLV_LENGTH));
    
    BUFFER_OFFSET += sizeof(SUB_TLV_LENGTH);
    memcpy(BUFFER_OFFSET, &SUB_TLV_VALUE, sizeof(SUB_TLV_VALUE));

    FRAME_HEADERS->RX_SUB_TLV_TYPE  = (uint16_t*) (FRAME_HEADERS->RX_DATA +
                                                   FRAME_HEADERS->TLV_SIZE);

    FRAME_HEADERS->RX_SUB_TLV_VALUE = (uint64_t*) (FRAME_HEADERS->RX_DATA +
                                                   FRAME_HEADERS->TLV_SIZE +
                                                   sizeof(SUB_TLV_TYPE) +
                                                   sizeof(SUB_TLV_LENGTH));

}



int16_t cli_args(int argc, char *argv[], struct APP_PARAMS *APP_PARAMS,
             struct FRAME_HEADERS *FRAME_HEADERS,
             struct TEST_INTERFACE *TEST_INTERFACE,
             struct TEST_PARAMS *TEST_PARAMS, struct MTU_TEST *MTU_TEST,
             struct QM_TEST *QM_TEST)
{

    if (argc > 1) 
    {

        for (uint16_t i = 1; i < argc; i += 1) 
        {

            // Change to receive mode
            if (strncmp(argv[i], "-r" ,2)==0) 
            {
                APP_PARAMS->TX_MODE = false;

                FRAME_HEADERS->SOURCE_MAC[0] = 0x00;
                FRAME_HEADERS->SOURCE_MAC[1] = 0x00;
                FRAME_HEADERS->SOURCE_MAC[2] = 0x5E;
                FRAME_HEADERS->SOURCE_MAC[3] = 0x00;
                FRAME_HEADERS->SOURCE_MAC[4] = 0x00;
                FRAME_HEADERS->SOURCE_MAC[5] = 0x02;

                FRAME_HEADERS->DEST_MAC[0]   = 0x00;
                FRAME_HEADERS->DEST_MAC[1]   = 0x00;
                FRAME_HEADERS->DEST_MAC[2]   = 0x5E;
                FRAME_HEADERS->DEST_MAC[3]   = 0x00;
                FRAME_HEADERS->DEST_MAC[4]   = 0x00;
                FRAME_HEADERS->DEST_MAC[5]   = 0x01;


            // Specifying a custom destination MAC address
            } else if (strncmp(argv[i], "-d", 2)==0) {
                uint8_t count;
                char    *tokens[6];
                char    delim[] = ":";

                count = explode_char(argv[i+1], delim, tokens);
                if (count == 6) {
                    for (uint8_t j = 0; j < 6; j += 1)
                    {
                        FRAME_HEADERS->DEST_MAC[j] = (uint8_t)strtoul(tokens[j], NULL, 16);
                    }

                } else {
                    printf("Error: Invalid destination MAC address!\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }
                i += 1;


            // Disable settings sync between TX and RX
            } else if (strncmp(argv[i], "-g", 2)==0) {
                APP_PARAMS->TX_SYNC = false;


            // Disable the TX to RX delay check
            } else if (strncmp(argv[i], "-G", 2)==0) {
                APP_PARAMS->TX_DELAY = 0;


            // Specifying a custom source MAC address
            } else if (strncmp(argv[i], "-s" ,2)==0) {
                uint8_t count;
                char    *tokens[6];
                char    delim[] = ":";

                count = explode_char(argv[i+1], delim, tokens);
                if (count == 6) {
                    for (uint8_t j = 0; j < 6; j += 1)
                    {
                        FRAME_HEADERS->SOURCE_MAC[j] = (uint8_t)strtoul(tokens[j], NULL, 16);
                    }

                } else {
                    printf("Error: Invalid source MAC address!\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }
                i += 1;


            // Specifying a custom interface name
            } else if (strncmp(argv[i], "-i", 2)==0) {
                if (argc > (i+1))
                {
                    uint8_t size = sizeof(argv[i+1]);
                    strncpy((char*)TEST_INTERFACE->IF_NAME,
                            argv[i+1], size);
                    i += 1;
                } else {
                    printf("Oops! Missing interface name\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }


            // Specifying a custom interface index
            } else if (strncmp(argv[i], "-I", 2)==0) {
                if (argc > (i+1))
                {
                    TEST_INTERFACE->IF_INDEX = (int)strtoul(argv[i+1], NULL, 0);
                    i += 1;
                } else {
                    printf("Oops! Missing interface index\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }


            // Requesting to list interfaces
            } else if (strncmp(argv[i], "-l", 2)==0) {
                list_interfaces();
                return RET_EXIT_APP;


            // Specifying a custom frame payload size in bytes
            } else if (strncmp(argv[i], "-f", 2)==0) {
                if (argc > (i+1))
                {
                    TEST_PARAMS->F_SIZE = (uint32_t)strtoul(argv[i+1], NULL, 0);
                    if (TEST_PARAMS->F_SIZE > 1500)
                    {
                        printf("WARNING: Make sure your device supports baby "
                               "giants or jumbo frames as required\n");
                    }
                    if (TEST_PARAMS->F_SIZE < 46) {
                        printf("WARNING: Minimum ethernet payload is 46 bytes, "
                               "Linux may pad the frame out to 46 bytes\n");
                        if (TEST_PARAMS->F_SIZE < (FRAME_HEADERS->SUB_TLV_SIZE))
                            TEST_PARAMS->F_SIZE = FRAME_HEADERS->SUB_TLV_SIZE;
                    }
                    
                    i += 1;

                } else {
                    printf("Oops! Missing frame size\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }


            // Specifying a custom transmission duration in seconds
            } else if (strncmp(argv[i], "-t", 2)==0) {
                if (argc > (i+1))
                {
                    TEST_PARAMS->F_DURATION = strtoull(argv[i+1], NULL, 0);
                    i += 1;
                } else {
                    printf("Oops! Missing transmission duration\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }


            // Specifying the total number of frames to send instead of duration
            } else if (strncmp(argv[i], "-c", 2)==0) {
                if (argc > (i+1))
                {
                    TEST_PARAMS->F_COUNT = strtoull(argv[i+1], NULL, 0);
                    i += 1;
                } else {
                    printf("Oops! Missing max frame count\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }


            // Load a custom frame from file for TX
            } else if (strncmp(argv[i], "-C", 2)==0) {
                if (argc > (i+1))
                {
                    FILE* F_PAYLOAD = fopen(argv[i+1], "r");
                    if (F_PAYLOAD==NULL){
                        printf("Opps! File loading error!\n");
                        return EX_SOFTWARE;
                    }

                    int FILE_RET = 0;
                    TEST_PARAMS->F_PAYLOAD_SIZE = 0;
                    while (FILE_RET != EOF &&
                          (TEST_PARAMS->F_PAYLOAD_SIZE < F_SIZE_MAX)) {

                        FILE_RET = fscanf(F_PAYLOAD, "%hhx",
                                          TEST_PARAMS->F_PAYLOAD + TEST_PARAMS->F_PAYLOAD_SIZE);

                        if (FILE_RET == EOF)
                            break;

                        TEST_PARAMS->F_PAYLOAD_SIZE +=1;
                    }
                    fclose(F_PAYLOAD);

                    printf("Using custom frame (%d octets loaded)\n",
                            TEST_PARAMS->F_PAYLOAD_SIZE);

                    // Disable initial broadcast
                    APP_PARAMS->BROADCAST = false;
                    // Disable settings sync
                    APP_PARAMS->TX_SYNC = false;
                    // Disable delay calculation
                    APP_PARAMS->TX_DELAY = false;

                    i += 1;
                } else {
                    printf("Oops! Missing filename\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }


            // Specifying the total number of bytes to send instead of duration
            } else if (strncmp(argv[i], "-b", 2)==0) {
                if (argc > (i+1))
                {
                    TEST_PARAMS->F_BYTES = strtoull(argv[i+1], NULL, 0);
                    i += 1;
                } else {
                    printf("Oops! Missing max byte transfer limit\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }


            // Enable ACK mode testing
            } else if (strncmp(argv[i], "-a", 2)==0) {
                TEST_PARAMS->F_ACK = true;


            // Specify the max frames per second rate
            } else if (strncmp(argv[i], "-F", 2)==0) {
                if (argc > (i+1))
                {
                    TEST_PARAMS->F_TX_SPEED_MAX = strtoul(argv[i+1], NULL, 0);
                    i += 1;
                } else {
                    printf("Oops! Missing max frame rate\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }


            // Limit TX rate to max bytes per second
            } else if (strncmp(argv[i], "-m", 2)==0) {
                if (argc > (i+1))
                {
                    TEST_PARAMS->B_TX_SPEED_MAX = strtoul(argv[i+1], NULL, 0);
                    i += 1;
                } else {
                    printf("Oops! Missing max TX rate\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }


            // Limit TX rate to max bits per second
            } else if (strncmp(argv[i], "-M", 2)==0) {
                if (argc > (i+1))
                {
                    TEST_PARAMS->B_TX_SPEED_MAX = (uint32_t)floor(strtoul(argv[i+1], NULL, 0) / 8);
                    i += 1;
                } else {
                    printf("Oops! Missing max TX rate\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }


            // Set 802.1q VLAN ID
            } else if (strncmp(argv[i], "-v", 2)==0) {
                if (argc > (i+1))
                {
                    FRAME_HEADERS->VLAN_ID = (uint16_t)atoi(argv[i+1]);
                    i += 1;
                } else {
                    printf("Oops! Missing 802.1p VLAN ID\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }


            // Set 802.1p PCP value
            } else if (strncmp(argv[i], "-p", 2)==0) {
                if (argc > (i+1))
                {
                    FRAME_HEADERS->PCP = (uint16_t)atoi(argv[i+1]);
                    i += 1;
                } else {
                    printf("Oops! Missing 802.1p PCP value\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }


            // Set the DEI bit on the inner VLAN
            } else if (strncmp(argv[i], "-x", 2)==0) {
                FRAME_HEADERS->VLAN_DEI = 1;


            // Set 802.1ad QinQ outer VLAN ID
            } else if (strncmp(argv[i], "-q", 2)==0) {
                if (argc > (i+1))
                {
                    FRAME_HEADERS->QINQ_ID = (uint16_t)atoi(argv[i+1]);
                    i += 1;
                } else {
                    printf("Oops! Missing 802.1ad QinQ outer VLAN ID\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }


            // Set 802.1ad QinQ outer PCP value
            } else if (strncmp(argv[i], "-o", 2)==0) {
                if (argc > (i+1))
                {
                    FRAME_HEADERS->QINQ_PCP = (uint16_t)atoi(argv[i+1]);
                    i += 1;
                } else {
                    printf("Oops! Missing 802.1ad QinQ outer PCP value\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }


            // Set the DEI bit on the outer VLAN
            } else if (strncmp(argv[i], "-X", 2)==0) {
                FRAME_HEADERS->QINQ_DEI = 1;


            // Set a custom ethertype
            } else if (strncmp(argv[i], "-e", 2)==0) {
                if (argc > (i+1))
                {
                    FRAME_HEADERS->ETHERTYPE = (uint16_t)strtol(argv[i+1], NULL, 16);
                    i += 1;
                } else {
                    printf("Oops! Missing ethertype value\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }


            // Destination and source MAC for next-hop MPLS LSR
            } else if (strncmp(argv[i], "-D", 2)==0) {
                uint8_t count;
                char    *tokens[6];
                char    delim[] = ":";

                count = explode_char(argv[i+1], delim, tokens);
                if (count == 6) {
                    for (uint8_t j = 0; j < 6; j += 1)
                    {
                        FRAME_HEADERS->LSP_DEST_MAC[j] = (uint8_t)strtoul(tokens[j], NULL, 16);
                    }

                } else {
                    printf("Error: Invalid destination LSR MAC address!\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }
                i += 1;

                count = explode_char(argv[i+1], delim, tokens);
                if (count == 6) {
                    for (uint8_t j = 0; j < 6; j += 1)
                    {
                        FRAME_HEADERS->LSP_SOURCE_MAC[j] = (uint8_t)strtoul(tokens[j], NULL, 16);
                    }

                } else {
                    printf("Error: Invalid source LSR MAC address!\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }
                i += 1;


            // Push an MPLS label onto the stack
            } else if (strncmp(argv[i], "-L", 2)==0) {
                if (argc > (i+3))
                {
                    if (FRAME_HEADERS->MPLS_LABELS < MPLS_LABELS_MAX) {

                        if((uint32_t)atoi(argv[i+1]) > 1048575) {
                            printf("Oops! MPLS label higher than 1,048,575\n");
                            return RET_EXIT_FAILURE;
                        }

                        if((uint16_t)atoi(argv[i+2]) > 7) {
                            printf("Oops! MPLS EXP higher than 7\n");
                            return RET_EXIT_FAILURE;
                        }

                        if((uint16_t)atoi(argv[i+3]) > 255) {
                            printf("Oops! MPLS TTL higher than 255\n");
                            return RET_EXIT_FAILURE;
                        }

                        FRAME_HEADERS->MPLS_LABEL[FRAME_HEADERS->MPLS_LABELS] =
                            (uint32_t)atoi(argv[i+1]);
                        
                        FRAME_HEADERS->MPLS_EXP[FRAME_HEADERS->MPLS_LABELS]   =
                            (uint16_t)atoi(argv[i+2]);
                        
                        FRAME_HEADERS->MPLS_TTL[FRAME_HEADERS->MPLS_LABELS]   =
                            (uint16_t)atoi(argv[i+3]);
                        
                        FRAME_HEADERS->MPLS_LABELS += 1;

                        i+=3;

                    } else {
                        printf("Oops! You have exceeded the maximum number of "
                               "MPLS labels (%d)\n", MPLS_LABELS_MAX);
                        return RET_EXIT_FAILURE;
                    }
                } else {
                    printf("Oops! Missing MPLS label values\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }


            // Push pseudowire control word atop the label stack
            } else if (strncmp(argv[i], "-P", 2)==0) {
                FRAME_HEADERS->PWE_CONTROL_WORD = 1;


            // Enable the MTU sweep test
            } else if (strncmp(argv[i], "-U", 2)==0) {
                if (argc > (i+2))
                {
                    MTU_TEST->MTU_TX_MIN = (uint16_t)atoi(argv[i+1]);
                    MTU_TEST->MTU_TX_MAX = (uint16_t)atoi(argv[i+2]);

                    if (MTU_TEST->MTU_TX_MAX > F_SIZE_MAX) { 
                        printf("MTU size can not exceed the maximum hard coded"
                               " size: %u\n", F_SIZE_MAX);
                             return RET_EXIT_FAILURE;
                    }

                    MTU_TEST->ENABLED = true;
                    i+=2;

                } else {
                    printf("Oops! Missing min/max MTU sizes\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }


            // Enable the link quality measurement tests
            } else if (strncmp(argv[i], "-Q", 2)==0) {
                if (argc > (i+2))
                {
                    QM_TEST->INTERVAL = (uint32_t)atoi(argv[i+1]);
                    QM_TEST->TIMEOUT =  (uint32_t)atoi(argv[i+2]);

                    if (QM_TEST->TIMEOUT > QM_TEST->INTERVAL) { 
                        printf("Oops! Echo timeout exceeded the interval\n");
                        return RET_EXIT_FAILURE;
                    }

                    // Convert to ns for use with timespec
                    QM_TEST->INTERVAL_NSEC = (QM_TEST->INTERVAL * 1000000) % 1000000000;
                    QM_TEST->INTERVAL_SEC =  ((QM_TEST->INTERVAL * 1000000) - QM_TEST->INTERVAL_NSEC) / 1000000000;
                    QM_TEST->TIMEOUT_NSEC =  (QM_TEST->TIMEOUT * 1000000) % 1000000000;
                    QM_TEST->TIMEOUT_SEC =   ((QM_TEST->TIMEOUT * 1000000) - QM_TEST->TIMEOUT_NSEC) / 1000000000;
                    QM_TEST->ENABLED =       true;
                    i+=2;

                } else {
                    printf("Oops! Missing interval and timeout values\n"
                           "Usage info: %s -h\n", argv[0]);
                    return RET_EXIT_FAILURE;
                }


            // Display version
            } else if (strncmp(argv[i], "-V", 2)==0 ||
                       strncmp(argv[i], "--version", 9)==0) {

                printf("Etherate version %s\n", APP_VERSION);
                return RET_EXIT_APP;


            // Display usage instructions
            } else if (strncmp(argv[i], "-h", 2)==0 ||
                       strncmp(argv[i], "--help", 6)==0) {

                print_usage(APP_PARAMS, FRAME_HEADERS);
                return RET_EXIT_APP;


            // Else the user entered an invalid argument
            } else {
                    printf("Oops! Invalid argument %s\n"
                           "Usage info: %s -h\n", argv[i], argv[0]);
                    return RET_EXIT_FAILURE;
            }


        }


    } 

    if (APP_PARAMS->TX_MODE == true)
    {
        printf("Running in TX mode\n");
    } else {
        printf("Running in RX mode\n");
    }

    return EXIT_SUCCESS;

}



uint8_t explode_char(char *string, char *delim, char *tokens[])
{

    // Stole this function from somewhere, can't remember where,
    // either way, props to someone else ¯\_(ツ)_/¯
    uint8_t count = 0;
    char   *token;
    char   *stringp;
 
    stringp = string;
    while (stringp != NULL) {
        token = strsep(&stringp, delim);
        tokens[count] = token;
        count += 1;
    }

    return count;

}



int32_t get_interface_mtu_by_name(struct TEST_INTERFACE *TEST_INTERFACE)
{

    struct ifreq ifr;

    const uint8_t *pIF_NAME = TEST_INTERFACE->IF_NAME;

    strcpy(ifr.ifr_name,(char*)pIF_NAME);

    if (ioctl(TEST_INTERFACE->SOCKET_FD, SIOCGIFMTU, &ifr)==0)
    {
        return ifr.ifr_mtu;
    }

    return RET_EXIT_FAILURE;

}



int16_t get_sock_interface(struct TEST_INTERFACE *TEST_INTERFACE)
{

    struct ifreq *ifr, *ifend;
    struct ifreq ifreq;
    struct ifconf ifc;
    struct ifreq ifs[MAX_IFS];

    ifc.ifc_len = sizeof(ifs);
    ifc.ifc_req = ifs;

    if (ioctl(TEST_INTERFACE->SOCKET_FD, SIOCGIFCONF, &ifc) < 0)
    {
        perror("Error: No compatible interfaces found ");
        return RET_EXIT_FAILURE;
    }

    ifend = ifs + (ifc.ifc_len / sizeof(struct ifreq));
    for (ifr = ifc.ifc_req; ifr < ifend; ifr += 1)
    {

        // Is this a packet capable device? (Doesn't work with AF_PACKET?)
        if (ifr->ifr_addr.sa_family == AF_INET)
        {

            // Try to get a typical ethernet adapter, not a bridge or virt interface
            if (strncmp(ifr->ifr_name, "eth",  3)==0 ||
                strncmp(ifr->ifr_name, "en",   2)==0 ||
                strncmp(ifr->ifr_name, "em",   3)==0 ||
                strncmp(ifr->ifr_name, "wlan", 4)==0 ||
                strncmp(ifr->ifr_name, "wlp",  3)==0 )
            {

                strncpy(ifreq.ifr_name,ifr->ifr_name,sizeof(ifreq.ifr_name));

                // Does this device even have hardware address?
                if (ioctl (TEST_INTERFACE->SOCKET_FD, SIOCGIFHWADDR, &ifreq) != 0)
                {
                    perror("Error: Device has no hardware address ");
                    return RET_EXIT_FAILURE;
                }

                // Copy MAC address before SIOCGIFINDEX
                char mac[6];
                memcpy(mac, ifreq.ifr_addr.sa_data, 6);

                if (ioctl(TEST_INTERFACE->SOCKET_FD, SIOCGIFINDEX, &ifreq) != 0) {
                    perror("Error: could get interface index ");
                    return RET_EXIT_FAILURE;
                }

                strncpy((char*)TEST_INTERFACE->IF_NAME, ifreq.ifr_name, IFNAMSIZ);

                printf("Using device %s with address "
                       "%02x:%02x:%02x:%02x:%02x:%02x, interface index %u\n",
                       ifreq.ifr_name,
                       (uint8_t)mac[0],
                       (uint8_t)mac[1],
                       (uint8_t)mac[2],
                       (uint8_t)mac[3],
                       (uint8_t)mac[4],
                       (uint8_t)mac[5],
                       ifreq.ifr_ifindex);

                return ifreq.ifr_ifindex;

            } // If eth* || en* || em* || wlan*

        } // If AF_INET

    } // Looping through interface count

    return RET_EXIT_FAILURE;

}



uint64_t htonll(uint64_t val)
{

    if (__BYTE_ORDER == __BIG_ENDIAN) return (val);
    else return __bswap_64(val);

}



void list_interfaces()
{

    struct ifreq ifreq;
    struct ifaddrs *ifaddr, *ifa;

    int32_t SOCKET_FD;
    SOCKET_FD = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

    if (getifaddrs(&ifaddr) == -1)
    {
        perror("Error: Could get interface list ");
        exit(EX_PROTOCOL);
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {

        if (ifa->ifa_addr == NULL) {
            continue;
        }

        // Does this interface sub address family support AF_PACKET
        if (ifa->ifa_addr->sa_family==AF_PACKET)
        {

            // Set the ifreq by interface name
            strncpy(ifreq.ifr_name,ifa->ifa_name,sizeof(ifreq.ifr_name));

            // Does this device have a hardware address?
            if (ioctl (SOCKET_FD, SIOCGIFHWADDR, &ifreq)==0)
            {

                // Copy the MAC before running SIOCGIFINDEX
                char mac[6];
                memcpy(mac, ifreq.ifr_addr.sa_data, 6);

                // Get the interface index
                ioctl(SOCKET_FD, SIOCGIFINDEX, &ifreq);

                printf("Device %s with address %02x:%02x:%02x:%02x:%02x:%02x, "
                       "has interface index %d\n",
                       ifreq.ifr_name,
                       (uint8_t)mac[0],
                       (uint8_t)mac[1],
                       (uint8_t)mac[2],
                       (uint8_t)mac[3],
                       (uint8_t)mac[4],
                       (uint8_t)mac[5],
                       ifreq.ifr_ifindex);

            }

        }

    }

    close(SOCKET_FD);
    freeifaddrs(ifaddr);

}



uint64_t ntohll(uint64_t val)
{

    if (__BYTE_ORDER == __BIG_ENDIAN) return (val);
    else return __bswap_64(val);

}



void print_usage (struct APP_PARAMS *APP_PARAMS,
                  struct FRAME_HEADERS *FRAME_HEADERS)
{

    printf ("Usage info; [Mode] [Destination] [Source] [Transport] [Shortcuts] [Other]\n"
            "[Mode]\n"
            "\t-r\tChange to receive (listening) mode.\n"
            "\t\tDefault is transmit mode.\n"
            "[Destination]\n"
            "\t-d\tSpecify a custom desctination MAC address, \n"
            "\t\t-d 11:22:33:44:55:66. Without this the TX host\n"
            "\t\tdefaults to 00:00:5E:00:00:02 and the RX host\n"
            "\t\tdefaults to :01.\n"
            "\t-g\tTX host skips settings synchronisation with RX host\n"
            "\t\tand begins transmitting regardless of RX state.\n"
            "\t-G\tTX host skips delay calcuation towards RX host\n"
            "\t\tand begins transmitting regardless of RX state.\n"
            "[Source]\n"
            "\t-s\tSpecify a custom source MAC address,\n"
            "\t\t-s 11:22:33:44:55:66. Without this the TX host\n"
            "\t\tdefaults to 00:00:5E:00:00:01 and the RX host\n"
            "\t\tdefaults to 02.\n"
            "\t-i\tSet interface by name. Without this option we guess which\n"
            "\t\tinterface to use.\n"
            "\t-I\tSet interface by index. Without this option we guess which\n"
            "\t\tinterface to use.\n"
            "\t-l\tList interface indexes (then quit) for use with -I option.\n"
            "[Test Options]\n"
            "\t-a\tAck mode, have the receiver ack each frame during the test\n"
            "\t\t(This will significantly reduce the speed of the test).\n"
            "\t-b\tNumber of bytes to send, default is %u, default behaviour\n"
            "\t\tis to wait for duration.\n"
            "\t\tOnly one of -t, -c or -b can be used, both override -t,\n"
            "\t\t-b overrides -c.\n"
            "\t-c\tNumber of frames to send, default is %u, default behaviour\n"
            "\t\tis to wait for duration.\n"
            "\t-C\tLoad a custom frame as hex from file\n"
            "\t\t-C ./tcp-syn.txt\n"
            "\t-e\tSet a custom ethertype, the default is 0x%04x.\n"
            "\t-f\tFrame payload size in bytes, default is %u\n"
            "\t\t(excluding headers, %u bytes on the wire).\n"
            "\t-F\tMax frames per/second to send, -F 83 (1Mbps).\n"
            "\t-m\tMax bytes per/second to send, -m 125000 (1Mps).\n"
            "\t-M\tMax bits per/second to send, -M 1000000 (1Mbps).\n"
            "\t-t\tTransmition duration, seconds as integer, default is %" PRIu64 ".\n"
            "[Transport]\n"
            "\t-v\tAdd an 802.1q VLAN tag. Not in the header by default.\n"
            "\t\tIf using a PCP value with -p a default VLAN of 0 is added.\n"
            "\t-p\tAdd an 802.1p PCP value from 1 to 7 using options -p 1 to\n"
            "\t\t-p 7. If more than one value is given, the highest is used.\n"
            "\t\tDefault is 0 if none specified.\n"
            "\t\t(If no 802.1q tag is set the VLAN 0 will be used).\n"
            "\t-x\tSet the DEI bit in the VLAN tag.\n"
            "\t-q\tAdd an outer Q-in-Q tag. If used without -v, 1 is used\n"
            "\t\tfor the inner VLAN ID.\n"
            "\t-o\tAdd an 802.1p PCP value to the outer Q-in-Q VLAN tag.\n"
            "\t\tIf no PCP value is specified and a Q-in-Q VLAN ID is,\n"
            "\t\t0 will be used. If no outer Q-in-Q VLAN ID is supplied this\n"
            "\t\toption is ignored. -o 1 to -o 7 like the -p option above.\n"
            "\t-X\tSet the DEI bit on the outer VLAN tag.\n"
            "\t-D\tSpecify a destination then source MAC for sending MPLS\n"
            "\t\ttagged frames to the next-hop LSR:\n"
            "\t\t-D 00:11:22:33:44:55 66:77:88:99:AA:BB\n"
            "\t-L\tPush an MPLS label onto the frame. The label, EXP value\n"
            "\t\tand TTL must be specified: -L 1122 0 255. Repeat the \n"
            "\t\toption to create a label stack from top to bottom. The last\n"
            "\t\tlabel will automatically have the bottom of stack bit set.\n"
            "\t-P\tPush a pseudowire control world on top of the MPLS\n"
            "\t\tlabel stack to emulate an Ethernet pseudowire.\n"
            "[Additonal Tests]\n"
            "\t-U\tSpecify a minimum and maximum MTU size in bytes then\n"
            "\t\tperform an MTU sweep on the link towards the RX host to\n"
            "\t\tfind the maximum size supported, -U 1400 1600\n"
            "\t-Q\tSpecify an echo interval and timeout value in millis\n"
            "\t\tthen measure link quality (RTT and jitter), using\n"
            "\t\t-Q 1000 1000 (default duration is %" PRIu64 ", see -t).\n"
            "[Other]\n"
            "\t-V|--version Display version\n"
            "\t-h|--help Display this help text\n",
            F_BYTES_DEF,
            F_COUNT_DEF,
            ETHERTYPE_DEF,
            F_SIZE_DEF,
            (F_SIZE_DEF+FRAME_HEADERS->LENGTH),
            F_DURATION_DEF,
            F_DURATION_DEF);

}



int16_t remove_interface_promisc(struct TEST_INTERFACE *TEST_INTERFACE)
{

    printf("Leaving promiscuous mode\n");

    strncpy(ethreq.ifr_name,(char*)TEST_INTERFACE->IF_NAME,IFNAMSIZ);

    if (ioctl(TEST_INTERFACE->SOCKET_FD,SIOCGIFFLAGS,&ethreq) == -1)
    {
        perror("Error getting socket flags, leaving promiscuous mode failed ");

        if (close(TEST_INTERFACE->SOCKET_FD) != 0) {
            perror("Error: Could't close socket file descriptor ");
        }

        return EX_SOFTWARE;
    }

    ethreq.ifr_flags &= ~IFF_PROMISC;

    if (ioctl(TEST_INTERFACE->SOCKET_FD,SIOCSIFFLAGS,&ethreq) == -1)
    {
        perror("Error setting socket flags, leaving promiscuous mode failed ");

        if (close(TEST_INTERFACE->SOCKET_FD) != 0) {
            perror("Error: Couldn't close socket file descriptor ");
        }
        return EX_SOFTWARE;
    }


    return EXIT_SUCCESS;

}



void reset_app(struct FRAME_HEADERS *FRAME_HEADERS,
               struct TEST_INTERFACE *TEST_INTERFACE,
               struct TEST_PARAMS *TEST_PARAMS, struct QM_TEST *QM_TEST)
{

    free(FRAME_HEADERS->RX_BUFFER);
    free(FRAME_HEADERS->TX_BUFFER);
    free(TEST_PARAMS->F_PAYLOAD);
    free(QM_TEST->pDELAY_RESULTS);
    if (TEST_INTERFACE->SOCKET_FD > 0) close(TEST_INTERFACE->SOCKET_FD);

}



int16_t set_interface_promisc(struct TEST_INTERFACE *TEST_INTERFACE)
{

    printf("Entering promiscuous mode\n");
    strncpy(ethreq.ifr_name,(char*)TEST_INTERFACE->IF_NAME,IFNAMSIZ);

    if (ioctl(TEST_INTERFACE->SOCKET_FD,SIOCGIFFLAGS,&ethreq) == -1) 
    {

        perror("Error: Getting socket flags failed when entering promiscuous mode ");
        close(TEST_INTERFACE->SOCKET_FD);
        return EX_SOFTWARE;

    }

    ethreq.ifr_flags|=IFF_PROMISC;

    if (ioctl(TEST_INTERFACE->SOCKET_FD,SIOCSIFFLAGS,&ethreq) == -1)
    {

        perror("Error: Setting socket flags failed when entering promiscuous mode ");
        close(TEST_INTERFACE->SOCKET_FD);
        return EX_SOFTWARE;

    }

    return EXIT_SUCCESS;

}



int16_t set_sock_interface_index(struct TEST_INTERFACE *TEST_INTERFACE)
{

    struct ifreq ifreq;
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr)==-1)
    {
        perror("getifaddrs ");
        return(RET_EXIT_FAILURE);
    }


    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {

        if (ifa->ifa_addr == NULL) {
            continue;
        }

        // Does this interface sub address family support AF_PACKET
        if (ifa->ifa_addr->sa_family == AF_PACKET)
        {

            // Set the ifreq by interface name
            strncpy(ifreq.ifr_name, ifa->ifa_name, sizeof(ifreq.ifr_name));

            // Does this device have a hardware address?
            if (ioctl (TEST_INTERFACE->SOCKET_FD, SIOCGIFHWADDR, &ifreq) ==0 )
            {

                // Copy the MAC before running SIOCGIFINDEX
                char mac[6];
                memcpy(mac, ifreq.ifr_addr.sa_data, 6);

                // Get the interface index
                ioctl(TEST_INTERFACE->SOCKET_FD, SIOCGIFINDEX, &ifreq);

                // Check if this is the interface index the user specified
                if (ifreq.ifr_ifindex == TEST_INTERFACE->IF_INDEX)
                {

                    // Get the interface name
                    strncpy((char*)TEST_INTERFACE->IF_NAME,
                            ifreq.ifr_name, IFNAMSIZ);

                    printf("Using device %s with address "
                           "%02x:%02x:%02x:%02x:%02x:%02x, interface index %u\n",
                           ifreq.ifr_name,
                           (uint8_t)mac[0],
                           (uint8_t)mac[1],
                           (uint8_t)mac[2],
                           (uint8_t)mac[3],
                           (uint8_t)mac[4],
                           (uint8_t)mac[5],
                           ifreq.ifr_ifindex);

                    freeifaddrs(ifaddr);

                    return ifreq.ifr_ifindex;

                }

            }

        }

    }

    freeifaddrs(ifaddr);

    return RET_EXIT_FAILURE;

}



int16_t set_sock_interface_name(struct TEST_INTERFACE *TEST_INTERFACE)
{

    struct ifreq ifreq;
    struct ifaddrs *ifaddr, *ifa;

    if (getifaddrs(&ifaddr)==-1)
    {
        perror("getifaddrs ");
        exit(EX_PROTOCOL);
    }

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {

        if (ifa->ifa_addr == NULL) {
            continue;
        }

        // Does this interface sub address family support AF_PACKET
        if (ifa->ifa_addr->sa_family == AF_PACKET)
        {

            // Set the ifreq by interface name
            strncpy(ifreq.ifr_name, ifa->ifa_name, sizeof(ifreq.ifr_name));

            // Does this device have a hardware address?
            if (ioctl (TEST_INTERFACE->SOCKET_FD, SIOCGIFHWADDR, &ifreq) == 0)
            {

                // Copy the MAC before running SIOCGIFINDEX
                char mac[6];
                memcpy(mac, ifreq.ifr_addr.sa_data, 6);

                // Check if this is the interface name the user specified
                if (strcmp(ifreq.ifr_name, (char*)TEST_INTERFACE->IF_NAME) == 0)
                {

                    // Get the interface index
                    ioctl(TEST_INTERFACE->SOCKET_FD, SIOCGIFINDEX, &ifreq);

                    TEST_INTERFACE->IF_INDEX = ifreq.ifr_ifindex;

                    printf("Using device %s with address "
                           "%02x:%02x:%02x:%02x:%02x:%02x, interface index %u\n",
                           ifreq.ifr_name,
                           (uint8_t)mac[0],
                           (uint8_t)mac[1],
                           (uint8_t)mac[2],
                           (uint8_t)mac[3],
                           (uint8_t)mac[4],
                           (uint8_t)mac[5],
                           ifreq.ifr_ifindex);

                    freeifaddrs(ifaddr);

                    return ifreq.ifr_ifindex;

                }

            }

        }

    }

    freeifaddrs(ifaddr);

    return RET_EXIT_FAILURE;

}



void signal_handler(int signal)
{

    // Send a dying gasp to the other host in case the application is ending
    // before the running test has finished

    struct FRAME_HEADERS *FRAME_HEADERS = pFRAME_HEADERS;
    struct TEST_INTERFACE *TEST_INTERFACE = pTEST_INTERFACE;
    struct TEST_PARAMS *TEST_PARAMS = pTEST_PARAMS;
    struct QM_TEST *QM_TEST = pQM_TEST;

    build_tlv(FRAME_HEADERS, htons(TYPE_APPLICATION), htonl(VALUE_DYINGGASP));

    int16_t TX_RET_VAL = sendto(TEST_INTERFACE->SOCKET_FD,
                                FRAME_HEADERS->TX_BUFFER,
                                FRAME_HEADERS->LENGTH+FRAME_HEADERS->TLV_SIZE,
                                0,
                                (struct sockaddr*)&TEST_INTERFACE->SOCKET_ADDRESS,
                                sizeof(TEST_INTERFACE->SOCKET_ADDRESS));

    if (TX_RET_VAL <= 0)
    {
        perror("Dying gasp error ");
    }

    printf("Leaving promiscuous mode\n");

    strncpy(ethreq.ifr_name, (char*)TEST_INTERFACE->IF_NAME,IFNAMSIZ);

    if (ioctl(TEST_INTERFACE->SOCKET_FD, SIOCGIFFLAGS, &ethreq) == -1) {
        perror("Error: Failed to get socket flags when leaving promiscuous mode ");
    }

    ethreq.ifr_flags &= ~IFF_PROMISC;

    if (ioctl(TEST_INTERFACE->SOCKET_FD, SIOCSIFFLAGS, &ethreq) == -1) {
        perror("Error: Setting socket flags when leaving promiscuous mode ");
    }

    if (TEST_INTERFACE->SOCKET_FD > 0) { 
        if (close(TEST_INTERFACE->SOCKET_FD) != 0) {
            perror("Error: Couldn't close file descriptor ");
        }
    }

    if (QM_TEST->TIME_RX_1    != NULL) free(QM_TEST->TIME_RX_1);
    if (QM_TEST->TIME_RX_2    != NULL) free(QM_TEST->TIME_RX_2);
    if (QM_TEST->TIME_RX_DIFF != NULL) free(QM_TEST->TIME_RX_DIFF);
    if (QM_TEST->TIME_TX_1    != NULL) free(QM_TEST->TIME_TX_1);
    if (QM_TEST->TIME_TX_2    != NULL) free(QM_TEST->TIME_TX_2);
    if (QM_TEST->TIME_TX_DIFF != NULL) free(QM_TEST->TIME_TX_DIFF);

    free (QM_TEST->pDELAY_RESULTS);
    free (FRAME_HEADERS->RX_BUFFER);
    free (FRAME_HEADERS->TX_BUFFER);
    free (TEST_PARAMS->F_PAYLOAD);

    exit(signal);

}



void sync_settings(struct APP_PARAMS *APP_PARAMS,
                   struct FRAME_HEADERS *FRAME_HEADERS,
                   struct TEST_INTERFACE *TEST_INTERFACE,
                   struct TEST_PARAMS * TEST_PARAMS,
                   struct MTU_TEST *MTU_TEST, struct QM_TEST *QM_TEST)
{

    int16_t TX_RET_VAL;
    int16_t RX_LEN;

    build_tlv(FRAME_HEADERS, htons(TYPE_SETTING), htonl(VALUE_SETTING_SUB_TLV));

    // Send test settings to the RX host
    if (APP_PARAMS->TX_MODE == true)
    {

        printf("\nSynchronising settings with RX host\n");

        // Disable the delay calculation
        if (APP_PARAMS->TX_DELAY != TX_DELAY_DEF)
        {

            build_sub_tlv(FRAME_HEADERS, htons(TYPE_TXDELAY),
                          htonll(APP_PARAMS->TX_DELAY));

            TX_RET_VAL = sendto(TEST_INTERFACE->SOCKET_FD,
                                FRAME_HEADERS->TX_BUFFER,
                                FRAME_HEADERS->LENGTH+FRAME_HEADERS->SUB_TLV_SIZE,
                                0,
                                (struct sockaddr*)&TEST_INTERFACE->SOCKET_ADDRESS,
                                sizeof(TEST_INTERFACE->SOCKET_ADDRESS));

            if (TX_RET_VAL <= 0)
            {
                perror("Error: Sync settings transmit error ");
                return;
            }


            printf("TX to RX delay calculation disabled\n");

        }

        // Testing with a custom ethertype
        if (FRAME_HEADERS->ETHERTYPE != ETHERTYPE_DEF)
        {

            build_sub_tlv(FRAME_HEADERS, htons(TYPE_ETHERTYPE),
                          htonll(FRAME_HEADERS->ETHERTYPE));

            TX_RET_VAL = sendto(TEST_INTERFACE->SOCKET_FD,
                                FRAME_HEADERS->TX_BUFFER,
                                FRAME_HEADERS->LENGTH+FRAME_HEADERS->SUB_TLV_SIZE,
                                0,
                                (struct sockaddr*)&TEST_INTERFACE->SOCKET_ADDRESS,
                                sizeof(TEST_INTERFACE->SOCKET_ADDRESS));

            if (TX_RET_VAL <= 0)
            {
                perror("Error: Sync settings transmit error ");
                return;
            }


            printf("Ethertype set to 0x%04hx\n", FRAME_HEADERS->ETHERTYPE);

        }

        // Testing with a custom frame size
        if (TEST_PARAMS->F_SIZE != F_SIZE_DEF)
        {

            build_sub_tlv(FRAME_HEADERS, htons(TYPE_FRAMESIZE),
                          htonll(TEST_PARAMS->F_SIZE));

            TX_RET_VAL = sendto(TEST_INTERFACE->SOCKET_FD,
                                FRAME_HEADERS->TX_BUFFER,
                                FRAME_HEADERS->LENGTH+FRAME_HEADERS->SUB_TLV_SIZE,
                                0,
                                (struct sockaddr*)&TEST_INTERFACE->SOCKET_ADDRESS,
                                sizeof(TEST_INTERFACE->SOCKET_ADDRESS));

            if (TX_RET_VAL <= 0)
            {
                perror("Error: Sync settings transmit error ");
                return;
            }


            printf("Frame size set to %u\n", TEST_PARAMS->F_SIZE);

        }

        // Testing with a custom duration
        if (TEST_PARAMS->F_DURATION != F_DURATION_DEF)
        {

            build_sub_tlv(FRAME_HEADERS, htons(TYPE_DURATION),
                          htonll(TEST_PARAMS->F_DURATION));

            TX_RET_VAL = sendto(TEST_INTERFACE->SOCKET_FD,
                                FRAME_HEADERS->TX_BUFFER,
                                FRAME_HEADERS->LENGTH+FRAME_HEADERS->SUB_TLV_SIZE,
                                0,
                                (struct sockaddr*)&TEST_INTERFACE->SOCKET_ADDRESS,
                                sizeof(TEST_INTERFACE->SOCKET_ADDRESS));

            if (TX_RET_VAL <= 0)
            {
                perror("Error: Sync settings transmit error ");
                return;
            }


            printf("Test duration set to %" PRIu64 "\n", TEST_PARAMS->F_DURATION);

        }

        // Testing with a custom frame count
        if (TEST_PARAMS->F_COUNT != F_COUNT_DEF)
        {

            build_sub_tlv(FRAME_HEADERS, htons(TYPE_FRAMECOUNT),
                          htonll(TEST_PARAMS->F_COUNT));

            TX_RET_VAL = sendto(TEST_INTERFACE->SOCKET_FD,
                                FRAME_HEADERS->TX_BUFFER,
                                FRAME_HEADERS->LENGTH+FRAME_HEADERS->SUB_TLV_SIZE,
                                0,
                                (struct sockaddr*)&TEST_INTERFACE->SOCKET_ADDRESS,
                                sizeof(TEST_INTERFACE->SOCKET_ADDRESS));

            if (TX_RET_VAL <= 0)
            {
                perror("Error: Sync settings transmit error ");
                return;
            }


            printf("Frame count set to %" PRIu64 "\n", TEST_PARAMS->F_COUNT);

        }

        // Testing with a custom byte limit
        if (TEST_PARAMS->F_BYTES != F_BYTES_DEF)
        {

            build_sub_tlv(FRAME_HEADERS, htons(TYPE_BYTECOUNT),
                          htonll(TEST_PARAMS->F_BYTES));

            TX_RET_VAL = sendto(TEST_INTERFACE->SOCKET_FD,
                                FRAME_HEADERS->TX_BUFFER,
                                FRAME_HEADERS->LENGTH+FRAME_HEADERS->SUB_TLV_SIZE,
                                0,
                                (struct sockaddr*)&TEST_INTERFACE->SOCKET_ADDRESS,
                                sizeof(TEST_INTERFACE->SOCKET_ADDRESS));

            if (TX_RET_VAL <= 0)
            {
                perror("Error: Sync settings transmit error ");
                return;
            }


            printf("Byte limit set to %" PRIu64 "\n", TEST_PARAMS->F_BYTES);

        }

        // Testing with a custom max speed limit
        if (TEST_PARAMS->B_TX_SPEED_MAX != B_TX_SPEED_MAX_DEF)
        {

            build_sub_tlv(FRAME_HEADERS, htons(TYPE_MAXSPEED),
                          htonll(TEST_PARAMS->B_TX_SPEED_MAX));

            TX_RET_VAL = sendto(TEST_INTERFACE->SOCKET_FD,
                                FRAME_HEADERS->TX_BUFFER,
                                FRAME_HEADERS->LENGTH+FRAME_HEADERS->SUB_TLV_SIZE,
                                0,
                                (struct sockaddr*)&TEST_INTERFACE->SOCKET_ADDRESS,
                                sizeof(TEST_INTERFACE->SOCKET_ADDRESS));

            if (TX_RET_VAL <= 0)
            {
                perror("Error: Sync settings transmit error ");
                return;
            }


            printf("Max TX speed set to %.2fMbps\n",
                   (((float)TEST_PARAMS->B_TX_SPEED_MAX * 8) / 1000 / 1000));

        }

        // Testing with a custom inner VLAN PCP value
        if (FRAME_HEADERS->PCP != PCP_DEF)
        {

            build_sub_tlv(FRAME_HEADERS, htons(TYPE_VLANPCP),
                          htonll(FRAME_HEADERS->PCP));

            TX_RET_VAL = sendto(TEST_INTERFACE->SOCKET_FD,
                                FRAME_HEADERS->TX_BUFFER,
                                FRAME_HEADERS->LENGTH+FRAME_HEADERS->SUB_TLV_SIZE,
                                0,
                                (struct sockaddr*)&TEST_INTERFACE->SOCKET_ADDRESS,
                                sizeof(TEST_INTERFACE->SOCKET_ADDRESS));

            if (TX_RET_VAL <= 0)
            {
                perror("Error: Sync settings transmit error ");
                return;
            }


            printf("Inner VLAN PCP value set to %hu\n", FRAME_HEADERS->PCP);

        }

        // Tesing with a custom QinQ PCP value
        if (FRAME_HEADERS->QINQ_PCP != QINQ_PCP_DEF)
        {

            build_sub_tlv(FRAME_HEADERS, htons(TYPE_QINQPCP),
                          htonll(FRAME_HEADERS->QINQ_PCP));

            TX_RET_VAL = sendto(TEST_INTERFACE->SOCKET_FD,
                                FRAME_HEADERS->TX_BUFFER,
                                FRAME_HEADERS->LENGTH+FRAME_HEADERS->SUB_TLV_SIZE,
                                0,
                                (struct sockaddr*)&TEST_INTERFACE->SOCKET_ADDRESS,
                                sizeof(TEST_INTERFACE->SOCKET_ADDRESS));

            if (TX_RET_VAL <= 0)
            {
                perror("Error: Sync settings transmit error ");
                return;
            }


            printf("QinQ VLAN PCP value set to %hu\n", FRAME_HEADERS->QINQ_PCP);

        }

        // Tell RX to run in ACK mode
        if (TEST_PARAMS->F_ACK == true)
        {

            build_sub_tlv(FRAME_HEADERS, htons(TYPE_ACKMODE),
                          htonll(TEST_PARAMS->F_ACK));

            TX_RET_VAL = sendto(TEST_INTERFACE->SOCKET_FD,
                                FRAME_HEADERS->TX_BUFFER,
                                FRAME_HEADERS->LENGTH+FRAME_HEADERS->SUB_TLV_SIZE,
                                0,
                                (struct sockaddr*)&TEST_INTERFACE->SOCKET_ADDRESS,
                                sizeof(TEST_INTERFACE->SOCKET_ADDRESS));

            if (TX_RET_VAL <= 0)
            {
                perror("Error: Sync settings transmit error ");
                return;
            }


            printf("ACK mode enabled\n");
        }

        // Tell RX an MTU sweep test will be performed
        if (MTU_TEST->ENABLED)
        {

            build_sub_tlv(FRAME_HEADERS, htons(TYPE_MTUTEST),
                          htonll(MTU_TEST->ENABLED));

            TX_RET_VAL = sendto(TEST_INTERFACE->SOCKET_FD,
                                FRAME_HEADERS->TX_BUFFER,
                                FRAME_HEADERS->LENGTH+FRAME_HEADERS->SUB_TLV_SIZE,
                                0,
                                (struct sockaddr*)&TEST_INTERFACE->SOCKET_ADDRESS,
                                sizeof(TEST_INTERFACE->SOCKET_ADDRESS));

            if (TX_RET_VAL <= 0)
            {
                perror("Error: Sync settings transmit error ");
                return;
            }


            build_sub_tlv(FRAME_HEADERS, htons(TYPE_MTUMIN),
                          htonll(MTU_TEST->MTU_TX_MIN));

            TX_RET_VAL = sendto(TEST_INTERFACE->SOCKET_FD,
                                FRAME_HEADERS->TX_BUFFER,
                                FRAME_HEADERS->LENGTH+FRAME_HEADERS->SUB_TLV_SIZE,
                                0,
                                (struct sockaddr*)&TEST_INTERFACE->SOCKET_ADDRESS,
                                sizeof(TEST_INTERFACE->SOCKET_ADDRESS));

            if (TX_RET_VAL <= 0)
            {
                perror("Error: Sync settings transmit error ");
                return;
            }


            build_sub_tlv(FRAME_HEADERS, htons(TYPE_MTUMAX),
                          htonll(MTU_TEST->MTU_TX_MAX));

            TX_RET_VAL = sendto(TEST_INTERFACE->SOCKET_FD,
                                FRAME_HEADERS->TX_BUFFER,
                                FRAME_HEADERS->LENGTH+FRAME_HEADERS->SUB_TLV_SIZE,
                                0,
                                (struct sockaddr*)&TEST_INTERFACE->SOCKET_ADDRESS,
                                sizeof(TEST_INTERFACE->SOCKET_ADDRESS));

            if (TX_RET_VAL <= 0)
            {
                perror("Error: Sync settings transmit error ");
                return;
            }


            printf("MTU sweep test enabled from %u to %u\n",
                   MTU_TEST->MTU_TX_MIN, MTU_TEST->MTU_TX_MAX);
        
        }

        // Tell RX the link quality tests will be performed
        if(QM_TEST->ENABLED)
        {

            build_sub_tlv(FRAME_HEADERS, htons(TYPE_QMTEST),
                          htonll(QM_TEST->ENABLED));

            TX_RET_VAL = sendto(TEST_INTERFACE->SOCKET_FD,
                                FRAME_HEADERS->TX_BUFFER,
                                FRAME_HEADERS->LENGTH+FRAME_HEADERS->SUB_TLV_SIZE,
                                0,
                                (struct sockaddr*)&TEST_INTERFACE->SOCKET_ADDRESS,
                                sizeof(TEST_INTERFACE->SOCKET_ADDRESS));

            if (TX_RET_VAL <= 0)
            {
                perror("Error: Sync settings transmit error ");
                return;
            }


            build_sub_tlv(FRAME_HEADERS, htons(TYPE_QMINTERVAL),
                          htonll(QM_TEST->INTERVAL));

            TX_RET_VAL = sendto(TEST_INTERFACE->SOCKET_FD,
                                FRAME_HEADERS->TX_BUFFER,
                                FRAME_HEADERS->LENGTH+FRAME_HEADERS->SUB_TLV_SIZE,
                                0,
                                (struct sockaddr*)&TEST_INTERFACE->SOCKET_ADDRESS,
                                sizeof(TEST_INTERFACE->SOCKET_ADDRESS));

            if (TX_RET_VAL <= 0)
            {
                perror("Error: Sync settings transmit error ");
                return;
            }


            build_sub_tlv(FRAME_HEADERS, htons(TYPE_QMTIMEOUT),
                          htonll(QM_TEST->TIMEOUT));

            TX_RET_VAL = sendto(TEST_INTERFACE->SOCKET_FD,
                                FRAME_HEADERS->TX_BUFFER,
                                FRAME_HEADERS->LENGTH+FRAME_HEADERS->SUB_TLV_SIZE,
                                0,
                                (struct sockaddr*)&TEST_INTERFACE->SOCKET_ADDRESS,
                                sizeof(TEST_INTERFACE->SOCKET_ADDRESS));

            if (TX_RET_VAL <= 0)
            {
                perror("Error: Sync settings transmit error ");
                return;
            }


            printf("Link quality tests enabled\n");

        }

        // Let the receiver know all settings have been sent
        build_tlv(FRAME_HEADERS, htons(TYPE_SETTING),
                  htonl(VALUE_SETTING_END));

        TX_RET_VAL = sendto(TEST_INTERFACE->SOCKET_FD,
                            FRAME_HEADERS->TX_BUFFER,
                            FRAME_HEADERS->LENGTH+FRAME_HEADERS->TLV_SIZE,
                            0,
                            (struct sockaddr*)&TEST_INTERFACE->SOCKET_ADDRESS,
                            sizeof(TEST_INTERFACE->SOCKET_ADDRESS));

        if (TX_RET_VAL <= 0)
        {
            perror("Error: Sync settings transmit error ");
            return;
        }


        printf("Settings have been synchronised\n\n");


    } else if (APP_PARAMS->TX_MODE == false) {


        printf("\nWaiting for settings from TX host\n");

        // In listening mode RX waits for each parameter to come through,
        // Until TX signals all settings have been sent
        uint8_t WAITING = true;

        while (WAITING)
        {

            RX_LEN = recvfrom(TEST_INTERFACE->SOCKET_FD,
                              FRAME_HEADERS->RX_BUFFER,
                              TEST_PARAMS->F_SIZE_TOTAL,
                              0, NULL, NULL);

            if (RX_LEN >0)
            {


                // All settings have been received
                if (ntohs(*FRAME_HEADERS->RX_TLV_TYPE) == TYPE_SETTING &&
                    ntohl(*FRAME_HEADERS->RX_TLV_VALUE) == VALUE_SETTING_END) {

                    WAITING = false;
                    printf("Settings have been synchronised\n\n");

                // TX has disabled the TX to RX one way delay calculation
                } else if (ntohs(*FRAME_HEADERS->RX_SUB_TLV_TYPE) == TYPE_TXDELAY) {

                    APP_PARAMS->TX_DELAY = (uint8_t)ntohll(*FRAME_HEADERS->RX_SUB_TLV_VALUE);
                    printf("TX to RX delay calculation disabled\n");

                // TX has sent a non-default ethertype
                } else if (ntohs(*FRAME_HEADERS->RX_SUB_TLV_TYPE) == TYPE_ETHERTYPE) {

                    FRAME_HEADERS->ETHERTYPE = (uint16_t)ntohll(*FRAME_HEADERS->RX_SUB_TLV_VALUE);
                    printf("Ethertype set to 0x%04hx\n", FRAME_HEADERS->ETHERTYPE);

                // TX has sent a non-default frame payload size
                } else if (ntohs(*FRAME_HEADERS->RX_SUB_TLV_TYPE) == TYPE_FRAMESIZE) {

                    TEST_PARAMS->F_SIZE = (uint16_t)ntohll(*FRAME_HEADERS->RX_SUB_TLV_VALUE);
                    printf("Frame size set to %hu\n", TEST_PARAMS->F_SIZE);

                // TX has sent a non-default transmition duration
                } else if (ntohs(*FRAME_HEADERS->RX_SUB_TLV_TYPE) == TYPE_DURATION) {

                    TEST_PARAMS->F_DURATION = (uint64_t)ntohll(*FRAME_HEADERS->RX_SUB_TLV_VALUE);
                    printf("Test duration set to %" PRIu64 "\n", TEST_PARAMS->F_DURATION);

                // TX has sent a frame count to use instead of duration
                } else if (ntohs(*FRAME_HEADERS->RX_SUB_TLV_TYPE) == TYPE_FRAMECOUNT) {

                    TEST_PARAMS->F_COUNT = (uint64_t)ntohll(*FRAME_HEADERS->RX_SUB_TLV_VALUE);
                    printf("Frame count set to %" PRIu64 "\n", TEST_PARAMS->F_COUNT);

                // TX has sent a total bytes value to use instead of frame count
                } else if (ntohs(*FRAME_HEADERS->RX_SUB_TLV_TYPE) == TYPE_BYTECOUNT) {

                    TEST_PARAMS->F_BYTES = (uint64_t)ntohll(*FRAME_HEADERS->RX_SUB_TLV_VALUE);
                    printf("Byte limit set to %" PRIu64 "\n", TEST_PARAMS->F_BYTES);

                // TX speed is limited
                } else if (ntohs(*FRAME_HEADERS->RX_SUB_TLV_TYPE) == TYPE_MAXSPEED) {

                    printf("Max TX speed set to %.2fMbps\n", ((float)*FRAME_HEADERS->RX_SUB_TLV_TYPE * 8) / 1000 / 1000);

                // TX has set a custom PCP value
                } else if (ntohs(*FRAME_HEADERS->RX_SUB_TLV_TYPE) == TYPE_VLANPCP) {

                    FRAME_HEADERS->PCP = (uint16_t)ntohll(*FRAME_HEADERS->RX_SUB_TLV_VALUE);
                    printf("PCP value set to %hu\n", FRAME_HEADERS->PCP);

                // TX has set a custom QinQ PCP value
                } else if (ntohs(*FRAME_HEADERS->RX_SUB_TLV_TYPE) == TYPE_QINQPCP) {

                    FRAME_HEADERS->QINQ_PCP = (uint16_t)ntohll(*FRAME_HEADERS->RX_SUB_TLV_VALUE);
                    printf("QinQ PCP value set to %hu\n", FRAME_HEADERS->QINQ_PCP);

                // TX has requested RX run in ACK mode
                } else if (ntohs(*FRAME_HEADERS->RX_SUB_TLV_TYPE) == TYPE_ACKMODE) {

                    TEST_PARAMS->F_ACK = true;
                    printf("ACK mode enabled\n");

                // TX has requested MTU sweep test
                } else if (ntohs(*FRAME_HEADERS->RX_SUB_TLV_TYPE) == TYPE_MTUTEST) {

                    MTU_TEST->ENABLED = true;
                    printf("MTU sweep test enabled\n");

                // TX has set MTU sweep test minimum MTU size
                } else if (ntohs(*FRAME_HEADERS->RX_SUB_TLV_TYPE) == TYPE_MTUMIN) {

                    MTU_TEST->MTU_TX_MIN = (uint16_t)ntohll(*FRAME_HEADERS->RX_SUB_TLV_VALUE);
                    printf("Minimum MTU set to %u\n", MTU_TEST->MTU_TX_MIN);

                // TX has set MTU sweep test maximum MTU size
                } else if (ntohs(*FRAME_HEADERS->RX_SUB_TLV_TYPE) == TYPE_MTUMAX) {

                    MTU_TEST->MTU_TX_MAX = (uint16_t)ntohll(*FRAME_HEADERS->RX_SUB_TLV_VALUE);
                    printf("Maximum MTU set to %u\n", MTU_TEST->MTU_TX_MAX);

                // TX has enabled link quality tests
                } else if (ntohs(*FRAME_HEADERS->RX_SUB_TLV_TYPE) == TYPE_QMTEST) {

                    QM_TEST->ENABLED = true;
                    printf("Link quality tests enabled\n");

                // TX has set echo interval
                } else if (ntohs(*FRAME_HEADERS->RX_SUB_TLV_TYPE) == TYPE_QMINTERVAL) {

                    // Convert to ns for use with timespec
                    QM_TEST->INTERVAL_NSEC =
                      (ntohll(*FRAME_HEADERS->RX_SUB_TLV_VALUE)*1000000)%1000000000;

                    QM_TEST->INTERVAL_SEC = 
                      ((ntohll(*FRAME_HEADERS->RX_SUB_TLV_VALUE)*1000000)-
                       QM_TEST->INTERVAL_NSEC)/1000000000;
                    
                // TX has set echo timeout
                } else if (ntohs(*FRAME_HEADERS->RX_SUB_TLV_TYPE) == TYPE_QMTIMEOUT) {

                    QM_TEST->TIMEOUT_NSEC =  (ntohll(*FRAME_HEADERS->RX_SUB_TLV_VALUE) * 1000000) % 1000000000;
                    QM_TEST->TIMEOUT_SEC =   ((ntohll(*FRAME_HEADERS->RX_SUB_TLV_VALUE) * 1000000) - QM_TEST->TIMEOUT_NSEC) / 1000000000;
                    
                }


            } else if (RX_LEN <0) {

                perror("Error: Setting receive failed ");
                {
                    return;
                }

            }

        } // WAITING

      
    } // TX or RX mode

}
