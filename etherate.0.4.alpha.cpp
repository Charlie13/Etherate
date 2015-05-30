/*
 * Etherate Main Body
 *
 * License: First rule of license club...
 *
 * Updates: https://github.com/jwbensley/Etherate and http://null.53bits.co.uk
 * Please send corrections, ideas and help to: jwbensley@gmail.com 
 * (I'm a beginner if that isn't obvious!)
 *
 * compile with: g++ -o etherate etherate.cpp -lrt
 *
 * File Contents:
 *
 * HEADERS AND DEFS
 * GLOBAL VARIABLES
 * CLI ARGS
 * TX/RX SETUP
 * SETTINGS SYNC
 * TEST PHASE
 *
 *
 ************************************************************* HEADERS AND DEFS
 */

#include <cmath> // std::abs() from cmath for floats and doubles
#include <cstring> // Needed for memcpy
#include <ctime> // Required for timer types
#include <fcntl.h> // fcntl()
#include <iomanip> //  setprecision() and precision()
#include <iostream> // cout
using namespace std;
#include <sys/socket.h> // Provides AF_PACKET *Address Family*
#include <sys/select.h>
#include <ifaddrs.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h> // Defines ETH_P_ALL [Ether type 0x003 (ALL PACKETS!)]
// Also defines ETH_FRAME_LEN with default 1514
// Also defines ETH_ALEN which is Ethernet Address Lenght in Octets (defined as 6)
#include <linux/if_packet.h>
//#include <linux/if.h>
#include <linux/net.h> // Provides SOCK_RAW *Socket Type*
//#include <net/if.h> // Provides if_indextoname/if_nametoindex
#include <netinet/in.h> // Needed for htons() and htonl()
#include <signal.h> // For use of signal() to catch SIGINT
#include <sstream> // Stringstream objects and functions
#include <stdio.h> //perror()
#include <stdlib.h> //malloc() itoa()
#include <stdbool.h> // bool/_Bool
#include <string.h> //Required for strncmp() function
#define MAX_IFS 64
#include "sysexits.h"
/*
 * EX_USAGE 64 command line usage error
 * EX_NOPERM 77 permission denied
 * EX_SOFTWARE 70 internal software error
 * EX_PROTOCOL 76 remote error in protocol
 */
#include <sys/ioctl.h>
#include <time.h> // Required for clock_gettime(), struct timeval
#include "unistd.h" // sleep(), getuid()
#include <vector> // Required for string explode function

// CLOCK_MONOTONIC_RAW is in from linux-2.26.28 onwards,
// It's needed here for accurate delay calculation
// There is a good S-O post about this here;
// http://stackoverflow.com/questions/3657289/linux-clock-gettimeclock-
// monotonic-strange-non-monotonic-behavior
#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW 4
#endif

#include "etherate.0.4.alpha.h"
#include "funcs.0.4.alpha.cpp"


/*
 ************************************************************* HEADERS AND DEFS
 */


int main(int argc, char *argv[]) {


    /*
     ********************************************************* GLOBAL VARIABLES
     */


    restart:

    // By default we are transmitting data
    bool txMode = true;

    // Interface index, default to -1 so we know later if the user changed it
    int IF_INDEX = IF_INDEX_DEF;
 
    // Source MAC address - Default is in IANA unassigned range
    SOURCE_MAC[0] = 0x00;
    SOURCE_MAC[1] = 0x00;
    SOURCE_MAC[2] = 0x5E;
    SOURCE_MAC[3] = 0x00;
    SOURCE_MAC[4] = 0x00;
    SOURCE_MAC[5] = 0x01;

    // Destination MAC address - Default is in IANA unassigned range
    DESTINATION_MAC[0] = 0x00;
    DESTINATION_MAC[1] = 0x00;
    DESTINATION_MAC[2] = 0x5E;
    DESTINATION_MAC[3] = 0x00;
    DESTINATION_MAC[4] = 0x00;
    DESTINATION_MAC[5] = 0x02;

    // This is the length of a frame header by default
    ETH_HEADERS_LEN = HEADERS_LEN_DEF;

    // The ETHERTYPE for the Etherate payload
    ETHERTYPE = ETHERTYPE_DEF;

    // Default 802.1p PCP/CoS value = 0
    PCP = PCP_DEF;

    // Default 802.1q VLAN ID = 0
    VLAN_ID = VLAN_ID_DEF;

    // Default 802.1ad VLAN ID of QinQ outer frame = 0
    QINQ_ID = QINQ_ID_DEF;

    // Default 802.1p PCP/CoS value of outer frame = 0
    QINQ_PCP = QINQ_PCP_DEF;

    // Frame payload in bytes
    int fSize = F_SIZE_DEF;

    // Total frame size including headers
    int fSizeTotal = F_SIZE_DEF + ETH_HEADERS_LEN;

    // Duration in seconds
    long long fDuration = F_DURATION_DEF;

    // Number of frames to send
    long long fCount = F_COUNT_DEF; 

    // Amount of data to transmit in bytes
    long long fBytes = F_BYTES_DEF;

    // Speed to transmit at (Max bytes per second)
    long bTXSpeed = B_TX_SPEED_DEF;

    // How fast we were transmitting for the last second
    long bTXSpeedLast = 0;

    // Total number of frames transmitted
    long long fTX = 0;

    // Frames sent at last count for stats
    long long fTXlast = 0;

    // Total number of frames received
    long long fRX = 0;

    // Frames received at last count for stats
    long long fRXlast = 0;

    // Index of the current test frame sent/received;
    long long fIndex = 0;

    // Index of the last test frame sent/received;
    long long fIndexLast = 0;

    // Frames received on time
    long long fOnTime = 0;

    // Frames received out of order that are early
    long long fEarly = 0;

    // Frames received out of order that are late
    long long fLate = 0;

    // Number of non test frames received
    long fRXother = 0;

    // Total number of bytes transmitted
    long long bTX = 0;

    // Bytes sent at last count for calculating speed
    long long bTXlast = 0;

    // Total number of bytes received
    long long bRX = 0;

    // Bytes received at last count for calculating speed
    long long bRXlast = 0;

    // Max speed whilst running the test
    float bSpeed = 0;

    // Are we running in ACK mode during transmition
    bool fACK = false;

    // Are we waiting for an ACK frame
    bool fWaiting = false;

    // Timeout to wait for a frame ACK
    timespec tsACK;

    // TX host waists to sync settings with RX or skip to transmit
    bool txSync = true;

    // These timespecs are used for calculating delay/RTT
    timespec tsRTT;

    // 5 doubles to calculate the delay 5 times, to get an average
    double delay[5];

    // Two timers for timing the test and calculating stats
    timespec tsCurrent, tsElapsed;

    // Seconds the test has been running
    long long sElapsed = 0;

    // Timer used for rate limiting the TX host
    timespec txLimit;

    // Time type for holding the current date and time
    time_t timeNow;

    // Time struct for breaking down the above time type
    tm* localtm;

    // Elapsed time struct for polling the socket file descriptor
    struct timeval tvSelectDelay;

    // A set of socket file descriptors for polling
    fd_set readfds;

    // Indicator for MTU sweep mode
    bool MTU_SWEEP = false;

    // Maximum MTU size to try up to
    int MTU_SWEEP_SIZE = 1500;

    /* 
      These variables are declared here and used over and over throughout;

       The string explode function is used several times throughout,
       so rather than continuously defining variables, they are defined here 
    */
    vector<string> exploded;
    string explodestring;

    // Also, a generic loop counter
    int lCounter;


    /*
     ********************************************************* GLOBAL VARIABLES
     */




    /*
     ***************************************************************** CLI ARGS
     */


    if(argc>1) 
    {

        for (lCounter = 1; lCounter < argc; lCounter++) 
        {

            // Change to receive mode
            if(strncmp(argv[lCounter],"-r",2)==0) 
            {
                txMode = false;

                SOURCE_MAC[0] = 0x00;
                SOURCE_MAC[1] = 0x00;
                SOURCE_MAC[2] = 0x5E;
                SOURCE_MAC[3] = 0x00;
                SOURCE_MAC[4] = 0x00;
                SOURCE_MAC[5] = 0x02;

                DESTINATION_MAC[0] = 0x00;
                DESTINATION_MAC[1] = 0x00;
                DESTINATION_MAC[2] = 0x5E;
                DESTINATION_MAC[3] = 0x00;
                DESTINATION_MAC[4] = 0x00;
                DESTINATION_MAC[5] = 0x01;


            // Specifying a custom destination MAC address
            } else if(strncmp(argv[lCounter],"-d",2)==0) {
                explodestring = argv[lCounter+1];
                exploded.clear();
                string_explode(explodestring, ":", &exploded);

                if((int) exploded.size() != 6) 
                {
                    cout << "Error: Invalid destination MAC address!" << endl
                         << "Usage info: " << argv[0] << " -h" << endl;
                    return EX_USAGE;
                }
                cout << "Destination MAC ";

                // Here strtoul() is used to convert the interger value of c_str()
                // to a hex number by giving it the number base 16 (hexadecimal)
                // otherwise it would be atoi()
                for(int i = 0; (i < 6) && (i < exploded.size()); ++i)
                {
                    DESTINATION_MAC[i] = (unsigned char)strtoul(exploded[i].c_str(), NULL, 16);
                    cout << setw(2) << setfill('0') << hex << int(DESTINATION_MAC[i]) << ":";
                }
                cout << dec << endl;
                lCounter++;


            // Disable settings sync between TX and RX
            } else if(strncmp(argv[lCounter],"-g",2)==0) {
                txSync = false;


            // Specifying a custom source MAC address
            } else if(strncmp(argv[lCounter],"-s",2)==0) {
                explodestring = argv[lCounter+1];
                exploded.clear();
                string_explode(explodestring, ":", &exploded);

                if((int) exploded.size() != 6) 
                {
                    cout << "Error: Invalid source MAC address!" << endl
                         << "Usage info: " << argv[0] << " -h" << endl;
                    return EX_USAGE;
                }
                cout << "Custom source MAC ";

                for(int i = 0; (i < 6) && (i < exploded.size()); ++i)
                {
                    SOURCE_MAC[i] = (unsigned char)strtoul(exploded[i].c_str(), NULL, 16);
                    cout << setw(2) << setfill('0') << hex << int(SOURCE_MAC[i]) << ":";
                }
                cout << dec << endl;
                lCounter++;


            // Specifying a custom interface name
            } else if(strncmp(argv[lCounter],"-i",2)==0) {
                if (argc>(lCounter+1))
                {
                    strncpy(IF_NAME,argv[lCounter+1],sizeof(argv[lCounter+1]));
                    lCounter++;
                } else {
                    cout << "Oops! Missing interface name" << endl
                         << "Usage info: " << argv[0] << " -h" << endl;
                    return EX_USAGE;
                }


            // Specifying a custom interface index
            } else if(strncmp(argv[lCounter],"-I",2)==0) {
                if (argc>(lCounter+1))
                {
                    IF_INDEX = atoi(argv[lCounter+1]);
                    lCounter++;
                } else {
                    cout << "Oops! Missing interface index" << endl
                         << "Usage info: " << argv[0] << " -h" << endl;
                    return EX_USAGE;
                }


            // Requesting to list interfaces
            } else if(strncmp(argv[lCounter],"-l",2)==0) {
                list_interfaces();
                return EXIT_SUCCESS;


            // Specifying a custom frame payload size in bytes
            } else if(strncmp(argv[lCounter],"-f",2)==0) {
                if (argc>(lCounter+1))
                {
                    fSize = atoi(argv[lCounter+1]);
                    if(fSize > 1500)
                    {
                        cout << "WARNING: Make sure your device supports baby giant"
                             << "or jumbo frames as required" << endl;
                    }
                    lCounter++;

                } else {
                    cout << "Oops! Missing frame size" << endl
                         << "Usage info: " << argv[0] << " -h" << endl;
                    return EX_USAGE;
                }


            // Specifying a custom transmission duration in seconds
            } else if(strncmp(argv[lCounter],"-t",2)==0) {
                if (argc>(lCounter+1))
                {
                    fDuration = atoll(argv[lCounter+1]);
                    lCounter++;
                } else {
                    cout << "Oops! Missing transmission duration" << endl
                         << "Usage info: " << argv[0] << " -h" << endl;
                    return EX_USAGE;
                }


            // Specifying the total number of frames to send instead of duration
            } else if(strncmp(argv[lCounter],"-c",2)==0) {
                if (argc>(lCounter+1))
                {
                    fCount = atoll(argv[lCounter+1]);
                    lCounter++;
                } else {
                    cout << "Oops! Missing max frame count" << endl
                         << "Usage info: " << argv[0] << " -h" << endl;
                    return EX_USAGE;
                }


            // Specifying the total number of bytes to send instead of duration
            } else if(strncmp(argv[lCounter],"-b",2)==0) {
                if (argc>(lCounter+1))
                {
                    fBytes = atoll(argv[lCounter+1]);
                    lCounter++;
                } else {
                    cout << "Oops! Missing max byte transfer limit" << endl
                         << "Usage info: " << argv[0] << " -h" << endl;
                    return EX_USAGE;
                }


            // Enable ACK mode testing
            } else if(strncmp(argv[lCounter],"-a",2)==0) {
                fACK = true;


            // Limit TX rate to max bytes per second
            } else if(strncmp(argv[lCounter],"-m",2)==0) {
                if (argc>(lCounter+1))
                {
                    bTXSpeed = atol(argv[lCounter+1]);
                    lCounter++;
                } else {
                    cout << "Oops! Missing max TX rate" << endl
                         << "Usage info: " << argv[0] << " -h" << endl;
                    return EX_USAGE;
                }


            // Limit TX rate to max bits per second
            } else if(strncmp(argv[lCounter],"-M",2)==0) {
                if (argc>(lCounter+1))
                {
                    bTXSpeed = atol(argv[lCounter+1]) / 8;
                    lCounter++;
                } else {
                    cout << "Oops! Missing max TX rate" << endl
                         << "Usage info: " << argv[0] << " -h" << endl;
                    return EX_USAGE;
                }


            // Set 802.1p PCP value
            } else if(strncmp(argv[lCounter],"-p",2)==0) {
                if (argc>(lCounter+1))
                {
                    PCP = atoi(argv[lCounter+1]);
                    lCounter++;
                } else {
                    cout << "Oops! Missing 802.1p PCP value" << endl
                         << "Usage info: " << argv[0] << " -h" << endl;
                    return EX_USAGE;
                }


            // Set 802.1q VLAN ID
            } else if(strncmp(argv[lCounter],"-v",2)==0) {
                if (argc>(lCounter+1))
                {
                    VLAN_ID = atoi(argv[lCounter+1]);
                    lCounter++;
                } else {
                    cout << "Oops! Missing 802.1p VLAN ID" << endl
                         << "Usage info: " << argv[0] << " -h" << endl;
                    return EX_USAGE;
                }


            // Set 802.1ad QinQ outer VLAN ID
            } else if(strncmp(argv[lCounter],"-q",2)==0) {
                if (argc>(lCounter+1))
                {
                    QINQ_ID = atoi(argv[lCounter+1]);
                    lCounter++;
                } else {
                    cout << "Oops! Missing 802.1ad QinQ outer VLAN ID" << endl
                         << "Usage info: " << argv[0] << " -h" << endl;
                    return EX_USAGE;
                }


            // Set 802.1ad QinQ outer PCP value
            } else if(strncmp(argv[lCounter],"-o",2)==0){
                if (argc>(lCounter+1))
                {
                    QINQ_PCP = atoi(argv[lCounter+1]);
                    lCounter++;
                } else {
                    cout << "Oops! Missing 802.1ad QinQ outer PCP value" << endl
                         << "Usage info: " << argv[0] << " -h" << endl;
                    return EX_USAGE;
                }


            // Set a custom ETHERTYPE
            } else if(strncmp(argv[lCounter],"-e",2)==0){
                if (argc>(lCounter+1))
                {
                    ETHERTYPE = (int)strtoul(argv[lCounter+1], NULL, 16);
                    lCounter++;
                } else {
                    cout << "Oops! Missing ETHERTYPE value" << endl
                         << "Usage info: " << argv[0] << " -h" << endl;
                    return EX_USAGE;
                }


            // Run an MTU sweet
            } else if(strncmp(argv[lCounter],"-U",2)==0){
                if (argc>(lCounter+1))
                {
                    MTU_SWEEP_SIZE = atoi(argv[lCounter+1]);
                    if(MTU_SWEEP_SIZE > F_SIZE_MAX) { 
                    	cout << "MTU size can not exceed the maximum hard"
                    	     << " coded frame size: " << F_SIZE_MAX << endl;
                    	     return EX_USAGE;
                    }
                    MTU_SWEEP = true;
                    lCounter++;
                } else {
                    cout << "Oops! Missing max MTU size value" << endl
                         << "Usage info: " << argv[0] << " -h" << endl;
                    return EX_USAGE;
                }


            // Display version
            } else if(strncmp(argv[lCounter],"-V",2)==0 ||
                      strncmp(argv[lCounter],"--version",9)==0) {
                cout << "Etherate version " << version << endl;
                return EXIT_SUCCESS;


            // Display usage instructions
            } else if(strncmp(argv[lCounter],"-h",2)==0 ||
                      strncmp(argv[lCounter],"--help",6)==0) {
                print_usage();
                return EXIT_SUCCESS;
            }


        }

    }


    /*
     ***************************************************************** CLI ARGS
     */




    /*
     ************************************************************** TX/RX SETUP
     */


    // Check for root access, needed low level socket access we desire;
    if(getuid()!=0) {
        cout << "Must be root to use this program!" << endl;
        return EX_NOPERM;
    }


    SOCKET_FD = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    /* Here we opened a socket with three arguments;
     * Communication Domain = AF_PACKET
     * Type/Communication Semantics = SOCK_RAW
     * Protocol = ETH_P_ALL == 0x0003, which is everything, from if_ether.h)
     */


    if (SOCKET_FD<0 )
    {
      cout << "Error defining socket." << endl;
      perror("socket() ");
      close(SOCKET_FD);
      return EX_SOFTWARE;
    }


    // If the user has supplied an interface index try to use that
    if (IF_INDEX>0) {

        IF_INDEX = set_sock_interface_index(SOCKET_FD, IF_INDEX);
        if (IF_INDEX==0)
        {
            cout << "Error: Couldn't set interface with index, "
                 << "returned index was 0." << endl;
            return EX_SOFTWARE;
        }

    // Or if the user has supplied an interface name try to use that        
    } else if(strcmp(IF_NAME,"")!=0) {

        IF_INDEX = set_sock_interface_name(SOCKET_FD, *IF_NAME);
        if (IF_INDEX==0)
        {
            cout << "Error: Couldn't set interface index from name, "
                 << "returned index was 0." << endl;
            return EX_SOFTWARE;
        }

    // Try and guess the best guess an interface
    } else if (IF_INDEX==IF_INDEX_DEF) {

        IF_INDEX = get_sock_interface(SOCKET_FD);
        if (IF_INDEX==0)
        {
            cout << "Error: Couldn't find appropriate interface ID, returned ID was 0."
                 << "This is typically the loopback interface ID."
                 << "Try supplying a source MAC address with the -s option." << endl;
            return EX_SOFTWARE;
        }

    }


    // Link layer socket struct (socket_address)
    // RAW packet communication = PF_PACKET
    socket_address.sll_family   = PF_PACKET;	
    // We don't use a protocol above ethernet layer so use anything here
    socket_address.sll_protocol = htons(ETH_P_IP);
    // Index of the network device
    socket_address.sll_ifindex  = IF_INDEX;
    // sock_address.sll_IF_INDEX = if_nametoindex(your_interface_name);
    // ARP hardware identifier is ethernet
    socket_address.sll_hatype   = ARPHRD_ETHER;
    // Target is another host
    socket_address.sll_pkttype  = PACKET_OTHERHOST;
    // Layer 2 address length
    socket_address.sll_halen    = ETH_ALEN;		
    // Destination MAC Address
    socket_address.sll_addr[0]  = DESTINATION_MAC[0];		
    socket_address.sll_addr[1]  = DESTINATION_MAC[1];
    socket_address.sll_addr[2]  = DESTINATION_MAC[2];
    socket_address.sll_addr[3]  = DESTINATION_MAC[3];
    socket_address.sll_addr[4]  = DESTINATION_MAC[4];
    socket_address.sll_addr[5]  = DESTINATION_MAC[5];
     // Last 2 bytes not used in Ethernet
    socket_address.sll_addr[6]  = 0x00;
    socket_address.sll_addr[7]  = 0x00;

    //  RX buffer for incoming ethernet frames
    char* rxBuffer = (char*)operator new(F_SIZE_MAX);

    //  TX buffer for outgoing ethernet frames
    TX_BUFFER = (char*)operator new(F_SIZE_MAX);

    TX_ETHERNET_HEADER = (unsigned char*)TX_BUFFER;

    build_headers(TX_BUFFER, DESTINATION_MAC, SOURCE_MAC, ETHERTYPE, PCP, VLAN_ID, QINQ_ID, QINQ_PCP, ETH_HEADERS_LEN);

    // Userdata pointers in ethernet frames
    char* rxData = rxBuffer + ETH_HEADERS_LEN;
    TX_DATA = TX_BUFFER + ETH_HEADERS_LEN;

    // 0 is success exit code for sending frames
    TX_REV_VAL = 0;
    // Length of the received frame
    int rxLength = 0;


    // http://www.linuxjournal.com/article/4659?page=0,1
    cout << "Entering promiscuous mode" << endl;

    // Get the interface name
    strncpy(ethreq.ifr_name,IF_NAME,IFNAMSIZ);

    // Set the network card in promiscuos mode

    if (ioctl(SOCKET_FD,SIOCGIFFLAGS,&ethreq)==-1) 
    {
        cout << "Error getting socket flags, entering promiscuous mode failed." << endl;
        perror("ioctl() ");
        close(SOCKET_FD);
        return EX_SOFTWARE;
    }

    ethreq.ifr_flags|=IFF_PROMISC;

    if (ioctl(SOCKET_FD,SIOCSIFFLAGS,&ethreq)==-1)
    {
        cout << "Error setting socket flags, entering promiscuous mode failed." << endl;
        perror("ioctl() ");
        close(SOCKET_FD);
        return EX_SOFTWARE;
    }

    std::string param;
    std::stringstream ss;
    ss.setf(ios::fixed);
    ss.setf(ios::showpoint);
    ss.precision(9);
    cout<<fixed<<setprecision(9);

    // At this point, declare our sigint handler, from now on when the two hosts
    // start communicating it will be of use, the TX will signal RX to reset when it dies
    signal (SIGINT,signal_handler);

    printf("Source MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
           SOURCE_MAC[0],SOURCE_MAC[1],SOURCE_MAC[2],SOURCE_MAC[3],SOURCE_MAC[4],SOURCE_MAC[5]);
    printf("Destination MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
           DESTINATION_MAC[0],DESTINATION_MAC[1],DESTINATION_MAC[2],DESTINATION_MAC[3],DESTINATION_MAC[4],DESTINATION_MAC[5]);

    /* 
     * Before any communication happens between the local host and remote host, we must 
     * broadcast our whereabouts to ensure there is no initial loss of frames
     *
     * Set the dest MAC to broadcast (FF...FF) and build the frame like this,
     * then transmit three frames with a short brake between each.
     * Rebuild the frame headers with the actual dest MAC.
     */

    unsigned char broadMAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    build_headers(TX_BUFFER, broadMAC, SOURCE_MAC, ETHERTYPE, PCP, VLAN_ID, QINQ_ID, QINQ_PCP, ETH_HEADERS_LEN);
    rxData = rxBuffer + ETH_HEADERS_LEN;
    TX_DATA = TX_BUFFER + ETH_HEADERS_LEN;

    cout << "Sending gratuitous broadcasts..." << endl;
    for(lCounter=1; lCounter<=3; lCounter++)
    {
        param = "etheratepresence";
        strncpy(TX_DATA,param.c_str(),param.length());
        TX_REV_VAL = sendto(SOCKET_FD, TX_BUFFER, param.length()+ETH_HEADERS_LEN, 0, 
                     (struct sockaddr*)&socket_address, sizeof(socket_address));
        sleep(1);
    }

    build_headers(TX_BUFFER, DESTINATION_MAC, SOURCE_MAC, ETHERTYPE, PCP, VLAN_ID, QINQ_ID, QINQ_PCP, ETH_HEADERS_LEN);

    if(ETH_HEADERS_LEN==18)
    {
        rxData = rxBuffer + (ETH_HEADERS_LEN-4);
        TX_DATA = TX_BUFFER + ETH_HEADERS_LEN;
    } else if (ETH_HEADERS_LEN==22) {
        rxData = rxBuffer + (ETH_HEADERS_LEN-4);
        TX_DATA = TX_BUFFER + ETH_HEADERS_LEN;
    } else {
        rxData = rxBuffer + ETH_HEADERS_LEN;
        TX_DATA = TX_BUFFER + ETH_HEADERS_LEN;
    }
    fSizeTotal = fSize + ETH_HEADERS_LEN;


    /*
     ************************************************************** TX/RX SETUP
     */



    /*
     ************************************************************** SHORTCUTS
     */

    if (MTU_SWEEP) {

	    // Fill the test frame with some junk data
	    int junk = 0;
	    for (junk = 0; junk < fSize; junk++)
	    {
	        TX_DATA[junk] = (char)((int) 65); // ASCII 65 = A;
	        //(255.0*rand()/(RAND_MAX+1.0)));
	    }

        for(lCounter=1;lCounter<=MTU_SWEEP_SIZE;lCounter++) {

        	
        }
    }

    /*
     ************************************************************** SHORTCUTS
     */



    /*
     ************************************************************ SETTINGS SYNC
     */


    // Set up the test by communicating settings with the RX host receiver
    if(txMode==true && txSync==true)
    { // If we are the TX host...

        cout << "Running in TX mode, synchronising settings" << endl;

        // Testing with a custom frame size
        if(ETHERTYPE!=ETHERTYPE_DEF)
        {
            ss << "etherateETHERTYPE" << ETHERTYPE;
            param = ss.str();
            strncpy(TX_DATA,param.c_str(),param.length());
            TX_REV_VAL = sendto(SOCKET_FD, TX_BUFFER, param.length()+ETH_HEADERS_LEN, 0, 
                   (struct sockaddr*)&socket_address, sizeof(socket_address));
            printf("ETHERTYPE set to 0x%X\n", ETHERTYPE);
        }

        // Testing with a custom frame size
        if(fSize!=F_SIZE_DEF)
        {
            ss << "etheratesize" << fSize;
            param = ss.str();
            strncpy(TX_DATA,param.c_str(),param.length());
            TX_REV_VAL = sendto(SOCKET_FD, TX_BUFFER, param.length()+ETH_HEADERS_LEN, 0, 
                   (struct sockaddr*)&socket_address, sizeof(socket_address));
            cout << "Frame size set to " << fSize << endl;
        }


        // Testing with a custom duration
        if(fDuration!=F_DURATION_DEF)
        {
            ss << "etherateduration" << fDuration;
            param = ss.str();
            strncpy(TX_DATA,param.c_str(),param.length());
            TX_REV_VAL = sendto(SOCKET_FD, TX_BUFFER, param.length()+ETH_HEADERS_LEN, 0, 
                   (struct sockaddr*)&socket_address, sizeof(socket_address));
            cout << "Test duration set to " << fDuration << endl;
        }


        // Testing with a custom frame count
        if(fCount!=F_COUNT_DEF) {
            ss << "etheratecount" << fCount;
            param = ss.str();
            strncpy(TX_DATA,param.c_str(),param.length());
            TX_REV_VAL = sendto(SOCKET_FD, TX_BUFFER, param.length()+ETH_HEADERS_LEN, 0, 
                   (struct sockaddr*)&socket_address, sizeof(socket_address));
            cout << "Frame count set to " << fCount << endl;
        }


        // Testing with a custom byte limit
        if(fBytes!=F_BYTES_DEF) {
            ss << "etheratebytes" << fBytes;
            param = ss.str();
            strncpy(TX_DATA,param.c_str(),param.length());
            TX_REV_VAL = sendto(SOCKET_FD, TX_BUFFER, param.length()+ETH_HEADERS_LEN, 0, 
                   (struct sockaddr*)&socket_address, sizeof(socket_address));
            cout << "Byte limit set to " << fBytes << endl;
        }


        // Testing with a custom max speed limit
        if(bTXSpeed!=B_TX_SPEED_DEF)
        {
            ss << "etheratespeed" << bTXSpeed;
            param = ss.str();
            strncpy(TX_DATA,param.c_str(),param.length());
            TX_REV_VAL = sendto(SOCKET_FD, TX_BUFFER, param.length()+ETH_HEADERS_LEN, 0, 
                   (struct sockaddr*)&socket_address, sizeof(socket_address));
            cout << "Max TX speed set to " << ((bTXSpeed*8)/1000/1000) << "Mbps" << endl;
        }

        // Testing with a custom inner VLAN PCP value
        if(PCP!=PCP_DEF)
        {
            ss << "etheratepcp" << PCP;
            param = ss.str();
            strncpy(TX_DATA,param.c_str(),param.length());
            TX_REV_VAL = sendto(SOCKET_FD, TX_BUFFER, param.length()+ETH_HEADERS_LEN, 0, 
                   (struct sockaddr*)&socket_address, sizeof(socket_address));
            cout << "Inner VLAN PCP value set to " << PCP << endl;
        }

        // Tesing with a custom QinQ PCP value
        if(PCP!=PCP_DEF)
        {
            ss << "etherateQINQ_PCP" << QINQ_PCP;
            param = ss.str();
            strncpy(TX_DATA,param.c_str(),param.length());
            TX_REV_VAL = sendto(SOCKET_FD, TX_BUFFER, param.length()+ETH_HEADERS_LEN, 0, 
                   (struct sockaddr*)&socket_address, sizeof(socket_address));
            cout << "QinQ VLAN PCP value set to " << QINQ_PCP << endl;
        }

        // Tell the receiver to run in ACK mode
        if(fACK==true)
        {
            param = "etherateack";
            strncpy(TX_DATA,param.c_str(),param.length());
            TX_REV_VAL = sendto(SOCKET_FD, TX_BUFFER, param.length()+ETH_HEADERS_LEN, 0, 
                   (struct sockaddr*)&socket_address, sizeof(socket_address));
            cout << "ACK mode enabled" << endl;
        }


        // Send over the time for delay calculation between hosts,
        // We send it twice each time repeating this process multiple times to get an average;
        cout << "Calculating delay between hosts..." << endl;
        for(lCounter=0; lCounter<=4; lCounter++)
        {
            // The monotonic value is used here in case we are unlucky enough to
            // run this during and NTP update, so we stay consistent
            clock_gettime(CLOCK_MONOTONIC_RAW, &tsRTT);
            ss.str("");
            ss.clear();
            ss<<"etheratetime"<<lCounter<<"1:"<<tsRTT.tv_sec<<":"<<tsRTT.tv_nsec;
            param = ss.str();
            strncpy(TX_DATA,param.c_str(),param.length());
            TX_REV_VAL = sendto(SOCKET_FD, TX_BUFFER, param.length()+ETH_HEADERS_LEN, 0, 
                         (struct sockaddr*)&socket_address, sizeof(socket_address));

            clock_gettime(CLOCK_MONOTONIC_RAW, &tsRTT);
            ss.str("");
            ss.clear();
            ss<<"etheratetime"<<lCounter<<"2:"<<tsRTT.tv_sec<<":"<<tsRTT.tv_nsec;
            param = ss.str();
            strncpy(TX_DATA,param.c_str(),param.length());
            TX_REV_VAL = sendto(SOCKET_FD, TX_BUFFER, param.length()+ETH_HEADERS_LEN, 0, 
                         (struct sockaddr*)&socket_address, sizeof(socket_address));

            // After sending the 2nd time value we wait for the returned delay;
            bool waiting = true;
            ss.str("");
            ss.clear();
            ss<<"etheratedelay.";
            param = ss.str();

            while (waiting)
            {
                rxLength = recvfrom(SOCKET_FD, rxBuffer, fSizeTotal, 0, NULL, NULL);

                if(param.compare(0,param.size(),rxData,0,14)==0)
                {
                    exploded.clear();
                    explodestring = rxData;
                    string_explode(explodestring, ".", &exploded);
                    ss.str("");
                    ss.clear();
                    ss << exploded[1].c_str()<<"."<<exploded[2].c_str();
                    param = ss.str();
                    delay[lCounter] = strtod(param.c_str(), NULL);
                    waiting = false;
                }

            }

        }

        cout << "Tx to Rx delay calculated as " <<
             ((delay[0]+delay[1]+delay[2]+delay[3]+delay[4])/5)*1000 << "ms" << endl;
        // Let the receiver know all settings have been sent
        param = "etherateallset";
        strncpy(TX_DATA,param.c_str(),param.length());
        TX_REV_VAL = sendto(SOCKET_FD, TX_BUFFER, param.length()+ETH_HEADERS_LEN, 0, 
               (struct sockaddr*)&socket_address, sizeof(socket_address));
        cout << "Settings have been synchronised." << endl;


    } else if (txMode==false && txSync==true) {

        cout << "Running in RX mode, awaiting TX host" << endl;
        // In listening mode we start by waiting for each parameter to come through
        // So we start a loop until they have all been received
        bool waiting = true;
        // Used for grabing sent values after the starting string in the received buffer
        int diff;
        // These values are used to calculate the delay between TX and RX hosts
        double timeTX1,timeRX1,timeTX2,timeRX2,timeTXdiff,timeRXdiff,timeTXdelay;

        // In this loop we are grabbing each incoming frame and looking at the 
        // string that prefixes it, to state which variable the following value
        // will be for
        while(waiting)
        {

            rxLength = recvfrom(SOCKET_FD, rxBuffer, fSizeTotal, 0, NULL, NULL);

            // TX has sent a non-default ETHERTYPE
            if(strncmp(rxData,"etherateETHERTYPE",17)==0)
            {
                diff = (rxLength-17);
                ss << rxData;
                param = ss.str().substr(17,diff);
                ETHERTYPE = strtol(param.c_str(),NULL,10);
                printf("ETHERTYPE set to 0x%X\n", ETHERTYPE);

            }


            // TX has sent a non-default frame payload size
            if(strncmp(rxData,"etheratesize",12)==0)
            {
                diff = (rxLength-12);
                ss << rxData;
                param = ss.str().substr(12,diff);
                fSize = strtol(param.c_str(),NULL,10);
                cout << "Frame size set to " << fSize << endl;
            }


            // TX has sent a non-default transmition duration
            if(strncmp(rxData,"etherateduration",16)==0)
            {
                diff = (rxLength-16);
                ss << rxData;
                param = ss.str().substr(16,diff);
                fDuration = strtoull(param.c_str(),0,10);
                cout << "Test duration set to " << fDuration << endl;
            }


            // TX has sent a frame count to use instead of duration
            if(strncmp(rxData,"etheratecount",13)==0)
            {
                diff = (rxLength-13);
                ss << rxData;
                param = ss.str().substr(13,diff);
                fCount = strtoull(param.c_str(),0,10);
                cout << "Frame count set to " << fCount << endl;
            }


            // TX has sent a total bytes value to use instead of frame count
            if(strncmp(rxData,"etheratebytes",13)==0)
            {
                diff = (rxLength-13);
                ss << rxData;
                param = ss.str().substr(13,diff);
                fBytes = strtoull(param.c_str(),0,10);
                cout << "Byte limit set to " << fBytes << endl;
            }

            // TX has set a custom PCP value
            if(strncmp(rxData,"etheratepcp",11)==0)
            {
                diff = (rxLength-11);
                ss << rxData;
                param = ss.str().substr(11,diff);
                PCP = strtoull(param.c_str(),0,10);
                cout << "PCP value set to " << PCP << endl;
            }

            // TX has set a custom PCP value
            if(strncmp(rxData,"etherateQINQ_PCP",15)==0)
            {
                diff = (rxLength-15);
                ss << rxData;
                param = ss.str().substr(15,diff);
                QINQ_PCP = strtoull(param.c_str(),0,10);
                cout << "QinQ PCP value set to " << QINQ_PCP << endl;
            }

            // TX has requested we run in ACK mode
            if(strncmp(rxData,"etherateack",11)==0)
            {
                fACK = true;
                cout << "ACK mode enabled" << endl;
            }

            // Now begin the part to calculate the delay between TX and RX hosts.
            // Several time values will be exchanged to estimate the delay between
            // the two. Then the process is repeated two more times so and average
            // can be taken, which is shared back with TX;
            if( strncmp(rxData,"etheratetime01:",15)==0 ||
                strncmp(rxData,"etheratetime11:",15)==0 ||
                strncmp(rxData,"etheratetime21:",15)==0 ||
                strncmp(rxData,"etheratetime31:",15)==0 ||
                strncmp(rxData,"etheratetime41:",15)==0  )
            {

                // Get the time we are receiving TX's sent time figure
                clock_gettime(CLOCK_MONOTONIC_RAW, &tsRTT);
                ss.str("");
                ss.clear();
                ss << tsRTT.tv_sec << "." << tsRTT.tv_nsec;
                ss >> timeRX1;

                // Extract the sent time
                exploded.clear();
                explodestring = rxData;
                string_explode(explodestring, ":", &exploded);

                ss.str("");
                ss.clear();
                ss << atol(exploded[1].c_str()) << "." << atol(exploded[2].c_str());
                ss >> timeTX1;

            }


            if( strncmp(rxData,"etheratetime02:",15)==0 ||
                strncmp(rxData,"etheratetime12:",15)==0 ||
                strncmp(rxData,"etheratetime22:",15)==0 ||
                strncmp(rxData,"etheratetime32:",15)==0 ||
                strncmp(rxData,"etheratetime42:",15)==0    )
            {

                // Get the time we are receiving TXs 2nd sent time figure
                clock_gettime(CLOCK_MONOTONIC_RAW, &tsRTT);
                ss.str("");
                ss.clear();
                ss << tsRTT.tv_sec << "." << tsRTT.tv_nsec;
                ss >> timeRX2;

                // Extract the sent time
                exploded.clear();
                explodestring = rxData;
                string_explode(explodestring, ":", &exploded);

                ss.clear();
                ss.str("");
                ss << atol(exploded[1].c_str()) << "." << atol(exploded[2].c_str());
                ss >> timeTX2;

                // Calculate the delay
                timeTXdiff = timeTX2-timeTX1;
                timeTXdelay = (timeRX2-timeTXdiff)-timeRX1;

                if(strncmp(rxData,"etheratetime02:",15)==0) delay[0] = timeTXdelay;
                if(strncmp(rxData,"etheratetime12:",15)==0) delay[1] = timeTXdelay;
                if(strncmp(rxData,"etheratetime22:",15)==0) delay[2] = timeTXdelay;
                if(strncmp(rxData,"etheratetime32:",15)==0) delay[3] = timeTXdelay;
                if(strncmp(rxData,"etheratetime42:",15)==0) 
                { 
                    delay[4] = timeTXdelay;
                    cout << "Tx to Rx delay calculated as " <<
                         ((delay[0]+delay[1]+delay[2]+delay[3]+delay[4])/5)*1000
                         << "ms" << endl;
                }

                // Send it back to the TX host
                ss.clear();
                ss.str("");
                ss << "etheratedelay." << timeTXdelay;
                param = ss.str();
                strncpy(TX_DATA,param.c_str(), param.length());
                TX_REV_VAL = sendto(SOCKET_FD, TX_BUFFER, param.length()+ETH_HEADERS_LEN, 0, 
                                    (struct sockaddr*)&socket_address,
                                    sizeof(socket_address));
            }

            // All settings have been received
            if(strncmp(rxData,"etherateallset",14)==0)
            {
                waiting = false;
                cout<<"Settings have been synchronised"<<endl;
            }

        } // Waiting bool

        // Rebuild the test frame headers in case any settings have been changed
        // by the TX host
        build_headers(TX_BUFFER, DESTINATION_MAC, SOURCE_MAC, ETHERTYPE, PCP, VLAN_ID,
                      QINQ_ID, QINQ_PCP, ETH_HEADERS_LEN);
      
    } // TX or RX mode


    /*
     ************************************************************ SETTINGS SYNC
     */



    /*
     *************************************************************** TEST PHASE
     */

    

    // Fill the test frame with some junk data
    int junk = 0;
    for (junk = 0; junk < fSize; junk++)
    {
        TX_DATA[junk] = (char)((int) 65); // ASCII 65 = A;
        //(255.0*rand()/(RAND_MAX+1.0)));
    }


    timeNow = time(0);
    localtm = localtime(&timeNow);
    cout << endl << "Starting test on " << asctime(localtm) << endl;
    ss.precision(2);
    cout << fixed << setprecision(2);

    FD_ZERO(&readfds);
    int SOCKET_FDCount = SOCKET_FD + 1;
    int selectRetVal;

    if (txMode==true)
    {

        cout << "Seconds\t\tMbps TX\t\tMBs Tx\t\tFrmTX/s\t\tFrames TX" << endl;

        long long *testBase, *testMax;
        if (fBytes>0)
        {
            // We are testing until X bytes received
            testMax = &fBytes;
            testBase = &bTX;

        } else if (fCount>0) {
            // We are testing until X frames received
            testMax = &fCount;
            testBase = &fTX;

        } else if (fDuration>0) {
            // We are testing until X seconds has passed
            fDuration--;
            testMax = &fDuration;
            testBase = &sElapsed;
        }


        // Get clock time for the speed limit option,
        // get another to record the initial starting time
        clock_gettime(CLOCK_MONOTONIC_RAW, &txLimit);
        clock_gettime(CLOCK_MONOTONIC_RAW, &tsElapsed);

        // Main TX loop
        while (*testBase<=*testMax)
        {

            // Get the current time
            clock_gettime(CLOCK_MONOTONIC_RAW, &tsCurrent);

            // One second has passed
            if((tsCurrent.tv_sec-tsElapsed.tv_sec)>=1)
            {
                sElapsed++;
                bSpeed = (((float)bTX-(float)bTXlast)*8)/1000/1000;
                bTXlast = bTX;
                cout << sElapsed << "\t\t" << bSpeed << "\t\t" << (bTX/1000)/1000
                     << "\t\t" << (fTX-fTXlast) << "\t\t" << fTX << endl;
                fTXlast = fTX;
                tsElapsed.tv_sec = tsCurrent.tv_sec;
                tsElapsed.tv_nsec = tsCurrent.tv_nsec;
            }

            // Poll the socket file descriptor with select() for incoming frames
            tvSelectDelay.tv_sec = 0;
            tvSelectDelay.tv_usec = 000000;
            FD_SET(SOCKET_FD, &readfds);
            selectRetVal = select(SOCKET_FDCount, &readfds, NULL, NULL, &tvSelectDelay);
            if (selectRetVal > 0) {
                if (FD_ISSET(SOCKET_FD, &readfds)) {

                    rxLength = recvfrom(SOCKET_FD, rxBuffer, fSizeTotal, 0, NULL, NULL);
                    if(fACK)
                    {
                        param = "etherateack";
                        if(strncmp(rxData,param.c_str(),param.length())==0)
                        {
                            fRX++;
                            fWaiting = false;

                        } else {

                            // Check if RX host has sent a dying gasp
                            param = "etheratedeath";
                            if(strncmp(rxData,param.c_str(),param.length())==0)
                            {
                                timeNow = time(0);
                                localtm = localtime(&timeNow);
                                cout << "RX host is going down." << endl
                                     << "Ending test and resetting on "
                                     << asctime(localtm) << endl;
                                goto finish;

                            } else {
                                fRXother++;
                            }

                        }

                    } else {
                        
                        // Check if RX host has sent a dying gasp
                        param = "etheratedeath";
                        if(strncmp(rxData,param.c_str(),param.length())==0)
                        {
                            timeNow = time(0);
                            localtm = localtime(&timeNow);
                            cout << "RX host is going down." << endl
                                 << "Ending test and resetting on "
                                 << asctime(localtm) << endl;
                            goto finish;

                        } else {
                            fRXother++;
                        }
                        
                    }
                    
                }
            }

            // If it hasn't been 1 second yet
            if (tsCurrent.tv_sec-txLimit.tv_sec<1)
            {

                if(!fWaiting) {

                    // A max speed has been set
                    if(bTXSpeed!=B_TX_SPEED_DEF)
                    {

                        // Check if sending another frame keeps us under the max speed limit
                        if((bTXSpeedLast+fSize)<=bTXSpeed)
                        {

                            ss.clear();
                            ss.str("");
                            ss << "etheratetest:" << (fTX+1) <<  ":";
                            param = ss.str();
                            strncpy(TX_DATA,param.c_str(), param.length());
                            TX_REV_VAL = sendto(SOCKET_FD, TX_BUFFER, fSizeTotal, 0, 
                       	                        (struct sockaddr*)&socket_address,
                                                sizeof(socket_address));
                            fTX++;
                            bTX+=fSize;
                            bTXSpeedLast+=fSize;
                            if (fACK) fWaiting = true;

                        }

                    } else {

                        ss.clear();
                        ss.str("");
                        ss << "etheratetest:" << (fTX+1) <<  ":";
                        param = ss.str();
                        strncpy(TX_DATA,param.c_str(), param.length());
                        TX_REV_VAL = sendto(SOCKET_FD, TX_BUFFER, fSizeTotal, 0, 
                                            (struct sockaddr*)&socket_address,
                                            sizeof(socket_address));
                        fTX+=1;
                        bTX+=fSize;
                        bTXSpeedLast+=fSize;
                        if (fACK) fWaiting = true;
                    }

                }

            } else { // 1 second has passed

              bTXSpeedLast = 0;
              clock_gettime(CLOCK_MONOTONIC_RAW, &txLimit);

            } // End of txLimit.tv_sec<1

        }

        cout << "Test frames transmitted: " << fTX << endl
             << "Test frames received: " << fRX << endl
             << "Non test frames received: " << fRXother << endl;

        timeNow = time(0);
        localtm = localtime(&timeNow);
        cout << endl << "Ending test on " << asctime(localtm) << endl;


    // Else, we are in RX mode
    } else {

        cout << "Seconds\t\tMbps RX\t\tMBs Rx\t\tFrmRX/s\t\tFrames RX" << endl;

        long long *testBase, *testMax;

        // Are we testing until X bytes received
        if (fBytes>0)
        {
            testMax = &fBytes;
            testBase = &bRX;

        // Or are we testing until X frames received
        } else if (fCount>0) {
            testMax = &fCount;
            testBase = &fRX;

        // Or are we testing until X seconds has passed
        } else if (fDuration>0) {
            fDuration--;
            testMax = &fDuration;
            testBase = &sElapsed;
        }

        // Get the initial starting time
        clock_gettime(CLOCK_MONOTONIC_RAW, &tsElapsed);

        // Main RX loop
        while (*testBase<=*testMax)
        {

            clock_gettime(CLOCK_MONOTONIC_RAW, &tsCurrent);
            // If one second has passed
            if((tsCurrent.tv_sec-tsElapsed.tv_sec)>=1)
            {
                sElapsed++;
                bSpeed = float (((float)bRX-(float)bRXlast)*8)/1000/1000;
                bRXlast = bRX;
                cout << sElapsed << "\t\t" << bSpeed << "\t\t" << (bRX/1000)/1000 << "\t\t"
                     << (fRX-fRXlast) << "\t\t" << fRX << endl;
                fRXlast = fRX;
                tsElapsed.tv_sec = tsCurrent.tv_sec;
                tsElapsed.tv_nsec = tsCurrent.tv_nsec;
            }

            // Poll the socket file descriptor with select() to 
            // check for incoming frames
            tvSelectDelay.tv_sec = 0;
            tvSelectDelay.tv_usec = 000000;
            FD_SET(SOCKET_FD, &readfds);
            selectRetVal = select(SOCKET_FDCount, &readfds, NULL, NULL, &tvSelectDelay);
            if (selectRetVal > 0) {
                if (FD_ISSET(SOCKET_FD, &readfds))
                {

                    rxLength = recvfrom(SOCKET_FD, rxBuffer, fSizeTotal, 0, NULL, NULL);

                    // Check if this is an etherate test frame
                    param = "etheratetest";
                    if(strncmp(rxData,param.c_str(),param.length())==0)
                    {

                        // Update test stats
                        fRX++;
                        bRX+=(rxLength-ETH_HEADERS_LEN);

                        // Get the index of the received frame
                        exploded.clear();
                        explodestring = rxData;
                        string_explode(explodestring, ":", &exploded);
                        fIndex = atoi(exploded[1].c_str());


                        if(fIndex==(fRX) || fIndex==(fIndexLast+1))
                        {
                            fOnTime++;
                            fIndexLast++;
                        } else if (fIndex>(fRX)) {
                            fIndexLast = fIndex;
                            fEarly++;
                        } else if (fIndex<fRX) {
                            fLate++;
                        }


                        // If running in ACK mode we need to ACK to TX host
                        if(fACK)
                        {

                            ss.clear();
                            ss.str("");
                            ss << "etherateack" << fRX;
                            param = ss.str();
                            strncpy(TX_DATA,param.c_str(), param.length());
                            TX_REV_VAL = sendto(SOCKET_FD, TX_BUFFER, fSizeTotal, 0, 
                                         (struct sockaddr*)&socket_address,
                                         sizeof(socket_address));
                            fTX++;
                            
                        }

                    } else {

                        // We received a non-test frame
                        fRXother++;

                    }

                    // Check if TX host has quit/died;
                    param = "etheratedeath";
                    if(strncmp(rxData,param.c_str(),param.length())==0)
                    {
                        timeNow = time(0);
                        localtm = localtime(&timeNow);
                        cout << "TX host is going down." << endl <<
                                "Ending test and resetting on " << asctime(localtm)
                                << endl;
                        goto restart;
                    }
                      
                }
            }

        }

        cout << "Test frames transmitted: " << fTX << endl
             << "Test frames received: " << fRX << endl
             << "Non test frames received: " << fRXother << endl
             << "In order test frames received: " << fOnTime << endl
             << "Out of order test frames received early: " << fEarly << endl
             << "Out of order frames received late: " << fLate << endl;

        timeNow = time(0);
        localtm = localtime(&timeNow);
        cout << endl << "Ending test on " << asctime(localtm) << endl;
        close(SOCKET_FD);
        goto restart;

    }


    finish:

    cout << "Leaving promiscuous mode" << endl;

    strncpy(ethreq.ifr_name,IF_NAME,IFNAMSIZ);

    if (ioctl(SOCKET_FD,SIOCGIFFLAGS,&ethreq)==-1)
    {
        cout << "Error getting socket flags, entering promiscuous mode failed." << endl;
        perror("ioctl() ");
        close(SOCKET_FD);
        return EX_SOFTWARE;
    }

    ethreq.ifr_flags &= ~IFF_PROMISC;

    if (ioctl(SOCKET_FD,SIOCSIFFLAGS,&ethreq)==-1)
    {
        cout << "Error setting socket flags, promiscuous mode failed." << endl;
        perror("ioctl() ");
        close(SOCKET_FD);
        return EX_SOFTWARE;
    }

    close(SOCKET_FD);


    /*
     *************************************************************** TEST PHASE
     */


    return EXIT_SUCCESS;

} // End of main()


