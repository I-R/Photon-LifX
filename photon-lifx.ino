/*
 LIFX bulb emulator by Kayne Richens (kayno@kayno.net)

 Emulates a LIFX bulb. Connect an RGB LED (or LED strip via drivers)
 to redPin, greenPin and bluePin as you normally would on an
 ethernet-ready Arduino and control it from the LIFX app!

 Notes:
 - Only one Client (e.g. app) can connect to the bulb at once

 Set the following variables below to suit your Arduino and network
 environment:
 - mac[device] (unique mac address for your arduino)
 - redPin (PWM pin for RED)
 - greenPin  (PWM pin for GREEN)
 - bluePin  (PWM pin for BLUE)

 Made possible by the work of magicmonkey:
 https://github.com/magicmonkey/lifxjs/ - you can use this to control
 your arduino bulb as well as real LIFX bulbs at the same time!

 And also the RGBMood library by Harold Waterkeyn, which was modified
 slightly to support powering down the LED
 */

#include "lifx.h"
#include "RGBMoodLifx.h"
#include "color.h"
#include "Adafruit_PWMServoDriver.h"


// Function declaration
void printLifxPacket(LifxPacket &pkt, unsigned int device);

unsigned int sendUDPPacket(LifxPacket &pkt, unsigned int device, IPAddress &remote_addr, int remote_port);

void sendPacket(LifxPacket &pkt, unsigned int device, IPAddress &remote_addr, int remote_port);

void processRequest(byte *packetBuffer, int packetSize, LifxPacket &request);

void handleRequest(LifxPacket &request, unsigned int device);

void setLight();

// set to 1 to output debug messages (including packet dumps) to serial (38400 baud)
const boolean DEBUG = 1;

// Number of Bulbs presented by this scetch
const unsigned int LifxBulbNum = 2;

// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[LifxBulbNum][6] = {
  {0xDE, 0xAD, 0xDE, 0xAD, 0xDE, 0xAD},
  {0xCE, 0xAC, 0xCE, 0xAC, 0xCE, 0xAC}};

byte site_mac[] = {
  0x4c, 0x49, 0x46, 0x58, 0x56, 0x32 }; // spells out "LIFXV2" - version 2 of the app changes the site address to this...

// pins for the RGB LED:
const int redPin = 4;
const int greenPin = 5;
const int bluePin = 6;

// label (name) for this bulb
char bulbLabel[LifxBulbNum][LifxBulbLabelLength] = {"Photon Bulb 1","Photon Bulb 2"};

// tags for this bulb
char bulbTags[LifxBulbNum][LifxBulbTagsLength] = {{0,0,0,0,0,0,0,0},{0,0,0,0,0,0,0,0}};
char bulbTagLabels[LifxBulbNum][LifxBulbTagLabelsLength] = {"",""};

// initial bulb values - warm white!
long power_status = 65535;
long hue = 0;
long sat = 0;
long bri = 65535;
long kel = 2000;
long dim = 0;

// Ethernet instances, for UDP broadcasting, and TCP server and Client
/*EthernetUDP Udp;
EthernetServer TcpServer = EthernetServer(LifxPort);
EthernetClient Client;*/

UDP Udp[LifxBulbNum];

RGBMoodLifx LIFXBulb(redPin, greenPin, bluePin);

String localIP;
int    PacketsRX=0;
String State="Undefined";

void setup() {

  Particle.variable("IP", localIP);
  Particle.variable("PacketsRX", PacketsRX);
  Particle.variable("State", State);

  State="setup";
  Wire.begin();

  Serial.begin(38400);
  Serial.println(F("LIFX bulb emulator for Arduino starting up..."));

    Serial.print(F("IP address for this bulb: "));
  localIP = WiFi.localIP();
  Serial.println( localIP );

  // set up a UDP and TCP port ready for incoming
  for(int i=0; i < LifxBulbNum; i++) {
    Udp[i].begin(LifxPort+i);
  };

  // set up the LED pins
  /*
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  */

  LIFXBulb.setFadingSteps(20);
  LIFXBulb.setFadingSpeed(20);

  // read in settings from EEPROM (if they exist) for bulb label and tags
  if(EEPROM.read(EEPROM_CONFIG_START) == EEPROM_CONFIG[0]
    && EEPROM.read(EEPROM_CONFIG_START+1) == EEPROM_CONFIG[1]
    && EEPROM.read(EEPROM_CONFIG_START+2) == EEPROM_CONFIG[2]) {
      if(DEBUG) {
        Serial.println(F("Config exists in EEPROM, reading..."));
        Serial.print(F("Bulb label: "));
      }
      for( int j = 0; j <LifxBulbNum; j++ ) {
        for(int i = 0; i < LifxBulbLabelLength; i++) {
          bulbLabel[j][i] = EEPROM.read(EEPROM_BULB_LABEL_START+i+EEPROM_BULB_CONFIG*j);

          if(DEBUG) {
            Serial.print(bulbLabel[j][i]);
          }
        }
      }

      if(DEBUG) {
        Serial.println();
        Serial.print(F("Bulb tags: "));
      }
      for( int j = 0; j <LifxBulbNum; j++ ) {
        for(int i = 0; i < LifxBulbTagsLength; i++) {
          bulbTags[j][i] = EEPROM.read(EEPROM_BULB_TAGS_START+i+EEPROM_BULB_CONFIG*j);

          if(DEBUG) {
            Serial.print(bulbTags[j][i]);
          }
        }
      }

      if(DEBUG) {
        Serial.println();
        Serial.print(F("Bulb tag labels: "));
      }
      for( int j = 0; j <LifxBulbNum; j++ ) {
        for(int i = 0; i < LifxBulbTagLabelsLength; i++) {
          bulbTagLabels[j][i] = EEPROM.read(EEPROM_BULB_TAG_LABELS_START+i+EEPROM_BULB_CONFIG*j);

          if(DEBUG) {
            Serial.print(bulbTagLabels[j][i]);
          }
        }
      }

      if(DEBUG) {
        Serial.println();
        Serial.println(F("Done reading EEPROM config."));
      }
  } else {
    // first time sketch has been run, set defaults into EEPROM
    if(DEBUG) {
      Serial.println(F("Config does not exist in EEPROM, writing..."));
    }

    EEPROM.write(EEPROM_CONFIG_START, EEPROM_CONFIG[0]);
    EEPROM.write(EEPROM_CONFIG_START+1, EEPROM_CONFIG[1]);
    EEPROM.write(EEPROM_CONFIG_START+2, EEPROM_CONFIG[2]);

    for( int j = 0; j <LifxBulbNum; j++ ) {
      for(int i = 0; i < LifxBulbLabelLength; i++) {
        EEPROM.write(EEPROM_BULB_LABEL_START+i+EEPROM_BULB_CONFIG*j, bulbLabel[j][i]);
      }
    }

    for( int j = 0; j <LifxBulbNum; j++ ) {
      for(int i = 0; i < LifxBulbTagsLength; i++) {
        EEPROM.write(EEPROM_BULB_TAGS_START+i+EEPROM_BULB_CONFIG*j, bulbTags[j][i]);
      }
    }

    for( int j = 0; j <LifxBulbNum; j++ ) {
      for(int i = 0; i < LifxBulbTagLabelsLength; i++) {
        EEPROM.write(EEPROM_BULB_TAG_LABELS_START+i+EEPROM_BULB_CONFIG*j, bulbTagLabels[j][i]);
      }
    }

    if(DEBUG) {
      Serial.println(F("Done writing EEPROM config."));
    }
  }

  if(DEBUG) {
    Serial.println(F("EEPROM dump:"));
    for(int i = 0; i < 256; i++) {
      Serial.print(EEPROM.read(i));
      Serial.print(SPACE);
    }
    Serial.println();
  }

  // set the bulb based on the initial colors
  setLight();
}

void loop() {
  State="loop";
  LIFXBulb.tick();

  // buffers for receiving and sending data
  byte PacketBuffer[128]; //buffer to hold incoming packet,

  // if there's UDP data available, read a packet
  for( int j=0; j<LifxBulbNum; j++){
    int packetSize = Udp[j].parsePacket();
    if(packetSize) {
      Udp[j].read(PacketBuffer, 128);
      PacketsRX++;

      if(DEBUG) {
        Serial.print(j,DEC);
        Serial.print(F(":-UDP (<-"));
        IPAddress remote_addr = Udp[j].remoteIP();
        for ( int i=0; i < 3; i++) {
          Serial.print(remote_addr[i]);
          Serial.print(F("."));
        }
        Serial.print(remote_addr[3]);

        Serial.print(F(":"));
        Serial.print(Udp[j].remotePort());
        Serial.print(F(") "));

        for(int i = 0; i < LifxPacketSize; i++) {
          Serial.print(PacketBuffer[i], HEX);
          Serial.print(SPACE);
        }

          for(int i = LifxPacketSize; i < packetSize; i++) {
          Serial.print(PacketBuffer[i], HEX);
          Serial.print(SPACE);
        }
        Serial.println();
      }

      // push the data into the LifxPacket structure
      LifxPacket request;
      processRequest(PacketBuffer, sizeof(PacketBuffer), request);

      //respond to the request
      handleRequest(request, j);
    }
  }


  //Ethernet.maintain();

  delay(10);
}


void processRequest(byte *packetBuffer, int packetSize, LifxPacket &request) {

  request.size        = packetBuffer[0] + (packetBuffer[1] << 8); //little endian
  request.protocol    = packetBuffer[2] + (packetBuffer[3] << 8); //little endian
  request.source[0]   = packetBuffer[4];
  request.source[1]   = packetBuffer[5];
  request.source[2]   = packetBuffer[6];
  request.source[3]   = packetBuffer[7];

  byte target[] = {
    packetBuffer[8], packetBuffer[9], packetBuffer[10],
    packetBuffer[11], packetBuffer[12], packetBuffer[13]
  };
  memcpy(request.target, target, 6);

  request.reserved2   = packetBuffer[14] + (packetBuffer[15]<<8);

  byte site[] = {
    packetBuffer[16], packetBuffer[17], packetBuffer[18],
    packetBuffer[19], packetBuffer[20], packetBuffer[21]
  };

  memcpy(request.site, site, 6);

  request.reserved3   = packetBuffer[22];
  request.sequence    = packetBuffer[23];
  Serial.print("S: ");
  Serial.println(request.sequence);
  byte reserved4[] = {
    packetBuffer[24], packetBuffer[25], packetBuffer[26], packetBuffer[27],
    packetBuffer[28], packetBuffer[29], packetBuffer[30], packetBuffer[31]
  };
  memcpy( request.reserved4 ,  reserved4, 8);
  request.packet_type = packetBuffer[32] + (packetBuffer[33] << 8); //little endian
  request.reserved5   = packetBuffer[34] + packetBuffer[35];

  int i;
  for(i = LifxPacketSize; i < packetSize; i++) {
    request.data[i-LifxPacketSize] = packetBuffer[i];
  }

  request.data_size = i;
}

void handleRequest(LifxPacket &request, unsigned int device) {
  if(DEBUG) {
    Serial.print(F("  Received packet type "));
    Serial.println(request.packet_type, DEC);
    Serial.print("Seq: ");
    Serial.println(request.sequence,DEC);
    Serial.print("Size: ");
    Serial.println(request.data_size,DEC);

  }

  LifxPacket response;

  response.sequence = request.sequence;
  memcpy(response.source,request.source,4);
  response.protocol = request.protocol;

  IPAddress remote_addr(Udp[device].remoteIP());
  int       remote_port = Udp[device].remotePort();
  switch(request.packet_type) {

  case GET_PAN_GATEWAY:
    {
      // we are a gateway, so respond to this
      for ( int i = 0 ; i < LifxBulbNum; i++ ) {
        // respond with the UDP port
        response.packet_type = PAN_GATEWAY;
        response.protocol = LifxProtocol_AllBulbsResponse;
        byte UDPdata[] = {
          SERVICE_UDP, //UDP
          lowByte(LifxPort+i),
          highByte(LifxPort+i),
          0x00,
          0x00
        };

        memcpy(response.data, UDPdata, sizeof(UDPdata));
        response.data_size = sizeof(UDPdata);
        sendPacket(response, i, remote_addr, remote_port);

        delay( 100 );
      }
    }

    break;


  case SET_LIGHT_STATE:
    {
      // set the light colors
      hue = word(request.data[2], request.data[1]);
      sat = word(request.data[4], request.data[3]);
      bri = word(request.data[6], request.data[5]);
      kel = word(request.data[8], request.data[7]);

      setLight();
    }
    break;


  case GET_LIGHT_STATE:
    {
      // send the light's state
      response.packet_type = LIGHT_STATUS;
      response.protocol = LifxProtocol_AllBulbsResponse;
      byte StateData[] = {
        lowByte(hue),  //hue
        highByte(hue), //hue
        lowByte(sat),  //sat
        highByte(sat), //sat
        lowByte(bri),  //bri
        highByte(bri), //bri
        lowByte(kel),  //kel
        highByte(kel), //kel
        lowByte(dim),  //dim
        highByte(dim), //dim
        lowByte(power_status),  //power status
        highByte(power_status), //power status
        // label
        lowByte(bulbLabel[device][0]),
        lowByte(bulbLabel[device][1]),
        lowByte(bulbLabel[device][2]),
        lowByte(bulbLabel[device][3]),
        lowByte(bulbLabel[device][4]),
        lowByte(bulbLabel[device][5]),
        lowByte(bulbLabel[device][6]),
        lowByte(bulbLabel[device][7]),
        lowByte(bulbLabel[device][8]),
        lowByte(bulbLabel[device][9]),
        lowByte(bulbLabel[device][10]),
        lowByte(bulbLabel[device][11]),
        lowByte(bulbLabel[device][12]),
        lowByte(bulbLabel[device][13]),
        lowByte(bulbLabel[device][14]),
        lowByte(bulbLabel[device][15]),
        lowByte(bulbLabel[device][16]),
        lowByte(bulbLabel[device][17]),
        lowByte(bulbLabel[device][18]),
        lowByte(bulbLabel[device][19]),
        lowByte(bulbLabel[device][20]),
        lowByte(bulbLabel[device][21]),
        lowByte(bulbLabel[device][22]),
        lowByte(bulbLabel[device][23]),
        lowByte(bulbLabel[device][24]),
        lowByte(bulbLabel[device][25]),
        lowByte(bulbLabel[device][26]),
        lowByte(bulbLabel[device][27]),
        lowByte(bulbLabel[device][28]),
        lowByte(bulbLabel[device][29]),
        lowByte(bulbLabel[device][30]),
        lowByte(bulbLabel[device][31]),
        //tags
        lowByte(bulbTags[device][0]),
        lowByte(bulbTags[device][1]),
        lowByte(bulbTags[device][2]),
        lowByte(bulbTags[device][3]),
        lowByte(bulbTags[device][4]),
        lowByte(bulbTags[device][5]),
        lowByte(bulbTags[device][6]),
        lowByte(bulbTags[device][7])
        };

      memcpy(response.data, StateData, sizeof(StateData));
      response.data_size = sizeof(StateData);
      sendPacket(response, device, remote_addr, remote_port);
    }
    break;


  case SET_POWER_STATE:
  case GET_POWER_STATE:
    {
      // set if we are setting
      if(request.packet_type == SET_POWER_STATE) {
        power_status = word(request.data[1], request.data[0]);
        setLight();
      }

      // respond to both get and set commands
      response.packet_type = POWER_STATE;
      response.protocol = LifxProtocol_AllBulbsResponse;
      byte PowerData[] = {
        lowByte(power_status),
        highByte(power_status)
        };

      memcpy(response.data, PowerData, sizeof(PowerData));
      response.data_size = sizeof(PowerData);
      sendPacket(response, device, remote_addr, remote_port);
    }
    break;


  case SET_BULB_LABEL:
  case GET_BULB_LABEL:
    {
      // set if we are setting
      if(request.packet_type == SET_BULB_LABEL) {
        for(int i = 0; i < LifxBulbLabelLength; i++) {
          if(bulbLabel[device][i] != request.data[i]) {
            bulbLabel[device][i] = request.data[i];
            EEPROM.write(EEPROM_BULB_LABEL_START+i, request.data[i]);
          }
        }
      }

      // respond to both get and set commands
      response.packet_type = BULB_LABEL;
      response.protocol = LifxProtocol_AllBulbsResponse;
      memcpy(response.data, bulbLabel[0], LifxBulbLabelLength);
      response.data_size = LifxBulbLabelLength;
      sendPacket(response, device, remote_addr, remote_port);
    }
    break;


  case SET_BULB_TAGS:
  case GET_BULB_TAGS:
    {
      // set if we are setting
      if(request.packet_type == SET_BULB_TAGS) {
        for(int i = 0; i < LifxBulbTagsLength; i++) {
          if(bulbTags[device][i] != request.data[i]) {
            bulbTags[device][i] = lowByte(request.data[i]);
            EEPROM.write(EEPROM_BULB_TAGS_START+i, request.data[i]);
          }
        }
      }

      // respond to both get and set commands
      response.packet_type = BULB_TAGS;
      response.protocol = LifxProtocol_AllBulbsResponse;
      memcpy(response.data, bulbTags, sizeof(bulbTags));
      response.data_size = sizeof(bulbTags);
      sendPacket(response, device, remote_addr, remote_port);
    }
    break;


  case SET_BULB_TAG_LABELS:
  case GET_BULB_TAG_LABELS:
    {
      // set if we are setting
      if(request.packet_type == SET_BULB_TAG_LABELS) {
        for(int i = 0; i < LifxBulbTagLabelsLength; i++) {
          if(bulbTagLabels[device][i] != request.data[i]) {
            bulbTagLabels[device][i] = request.data[i];
            EEPROM.write(EEPROM_BULB_TAG_LABELS_START+i, request.data[i]);
          }
        }
      }

      // respond to both get and set commands
      response.packet_type = BULB_TAG_LABELS;
      response.protocol = LifxProtocol_AllBulbsResponse;
      memcpy(response.data, bulbTagLabels, sizeof(bulbTagLabels));
      response.data_size = sizeof(bulbTagLabels);
      sendPacket(response, device, remote_addr, remote_port);
    }
    break;


  case GET_VERSION_STATE:
    {
      // respond to get command
      response.packet_type = VERSION_STATE;
      response.protocol = LifxProtocol_AllBulbsResponse;
      byte VersionData[] = {
        lowByte(LifxBulbVendor),
        highByte(LifxBulbVendor),
        0x00,
        0x00,
        lowByte(LifxBulbProduct),
        highByte(LifxBulbProduct),
        0x00,
        0x00,
        lowByte(LifxBulbVersion),
        highByte(LifxBulbVersion),
        0x00,
        0x00
        };

      memcpy(response.data, VersionData, sizeof(VersionData));
      response.data_size = sizeof(VersionData);
      sendPacket(response, device, remote_addr, remote_port);

      /*
      // respond again to get command (real bulbs respond twice, slightly diff data (see below)
      response.packet_type = VERSION_STATE;
      response.protocol = LifxProtocol_AllBulbsResponse;
      byte VersionData2[] = {
        lowByte(LifxVersionVendor), //vendor stays the same
        highByte(LifxVersionVendor),
        0x00,
        0x00,
        lowByte(LifxVersionProduct*2), //product is 2, rather than 1
        highByte(LifxVersionProduct*2),
        0x00,
        0x00,
        0x00, //version is 0, rather than 1
        0x00,
        0x00,
        0x00
        };

      memcpy(response.data, VersionData2, sizeof(VersionData2));
      response.data_size = sizeof(VersionData2);
      sendPacket(response, device, remote_addr, remote_port);
      */
    }
    break;


  case GET_MESH_FIRMWARE_STATE:
    {
      // respond to get command
      response.packet_type = MESH_FIRMWARE_STATE;
      response.protocol = LifxProtocol_AllBulbsResponse;
      // timestamp data comes from observed packet from a LIFX v1.5 bulb
      byte MeshVersionData[] = {
        0x00, 0x2e, 0xc3, 0x8b, 0xef, 0x30, 0x86, 0x13, //build timestamp
        0xe0, 0x25, 0x76, 0x45, 0x69, 0x81, 0x8b, 0x13, //install timestamp
        lowByte(LifxFirmwareVersionMinor),
        highByte(LifxFirmwareVersionMinor),
        lowByte(LifxFirmwareVersionMajor),
        highByte(LifxFirmwareVersionMajor)
        };

      memcpy(response.data, MeshVersionData, sizeof(MeshVersionData));
      response.data_size = sizeof(MeshVersionData);
      sendPacket(response, device, remote_addr, remote_port);
    }
    break;


  case GET_WIFI_FIRMWARE_STATE:
    {
      // respond to get command
      response.packet_type = WIFI_FIRMWARE_STATE;
      response.protocol = LifxProtocol_AllBulbsResponse;
      // timestamp data comes from observed packet from a LIFX v1.5 bulb
      byte WifiVersionData[] = {
        0x00, 0xc8, 0x5e, 0x31, 0x99, 0x51, 0x86, 0x13, //build timestamp
        0xc0, 0x0c, 0x07, 0x00, 0x48, 0x46, 0xd9, 0x43, //install timestamp
        lowByte(LifxFirmwareVersionMinor),
        highByte(LifxFirmwareVersionMinor),
        lowByte(LifxFirmwareVersionMajor),
        highByte(LifxFirmwareVersionMajor)
        };

      memcpy(response.data, WifiVersionData, sizeof(WifiVersionData));
      response.data_size = sizeof(WifiVersionData);
      sendPacket(response, device, remote_addr, remote_port);
    }
    break;


  default:
    {
      if(DEBUG) {
        Serial.println(F("  Unknown packet type, ignoring"));
      }
    }
    break;
  }
}

void sendPacket(LifxPacket &pkt, unsigned int device, IPAddress &remote_addr, int remote_port) {
  unsigned int ret;
  ret = sendUDPPacket(pkt, device, remote_addr, remote_port);
  /*
  if(Client.connected()) {
    sendTCPPacket(pkt);
  }
  */
  Serial.print("Sent: ");
  Serial.println( ret, DEC );
}

unsigned int sendUDPPacket(LifxPacket &pkt, unsigned int device, IPAddress &remote_addr, int remote_port) {
  // broadcast packet on local subnet
  //IPAddress remote_addr(Udp[device].remoteIP());
  IPAddress broadcast_addr(remote_addr[0], remote_addr[1], remote_addr[2], 255);

  if(DEBUG) {
    Serial.print(F("+UDP (->"));
    for ( int i=0; i < 3; i++) {
      Serial.print(remote_addr[i]);
      Serial.print(F("."));
    }
    Serial.print(remote_addr[3]);

    Serial.print(F(":"));
    Serial.print(Udp[device].remotePort());
    Serial.print(F(") "));
    printLifxPacket(pkt, device);
    Serial.println();
  }

  // Udp[device].beginPacket(broadcast_addr, Udp[device].remotePort());
  Udp[device].beginPacket(remote_addr, remote_port);

  // size
  Udp[device].write(lowByte(LifxPacketSize + pkt.data_size));
  Udp[device].write(highByte(LifxPacketSize + pkt.data_size));

  // protocol
  Udp[device].write(lowByte(pkt.protocol));
  Udp[device].write(highByte(pkt.protocol));

  // source
  //Udp[device].write(pkt.source,4);
  for(int i = 0; i < sizeof(pkt.source); i++) {
    Udp[device].write(lowByte(pkt.source[i]));
  }

/*
  Udp[device].write(lowByte(0x00));
  Udp[device].write(lowByte(0x00));
  Udp[device].write(lowByte(0x00));
  Udp[device].write(lowByte(0x00));
*/
  // target mac[device] address
  for(int i = 0; i < sizeof(mac[device]); i++) {
    Udp[device].write(lowByte(mac[device][i]));
  }

  // reserved2
  Udp[device].write(lowByte(0x00));
  Udp[device].write(lowByte(0x00));

  // site mac address
  for(int i = 0; i < sizeof(site_mac); i++) {
    Udp[device].write(lowByte(0x00));
    //Udp[device].write(lowByte(site_mac[i]));
  }

  // reserved3
  Udp[device].write(lowByte(0x00));
  Udp[device].write(lowByte(pkt.sequence));
  Serial.println(pkt.sequence, DEC);

  // reserved4
  Udp[device].write(lowByte(0x00));
  Udp[device].write(lowByte(0x00));
  Udp[device].write(lowByte(0x00));
  Udp[device].write(lowByte(0x00));
  Udp[device].write(lowByte(0x00));
  Udp[device].write(lowByte(0x00));
  Udp[device].write(lowByte(0x00));
  Udp[device].write(lowByte(0x00));

  //packet type
  Udp[device].write(lowByte(pkt.packet_type));
  Udp[device].write(highByte(pkt.packet_type));

  // reserved5
  Udp[device].write(lowByte(0x00));
  Udp[device].write(lowByte(0x00));

  //data
  for(int i = 0; i < pkt.data_size; i++) {
    Udp[device].write(lowByte(pkt.data[i]));
  }

  Udp[device].endPacket();

  return LifxPacketSize + pkt.data_size;
}

// print out a LifxPacket data structure as a series of hex bytes - used for DEBUG
void printLifxPacket(LifxPacket &pkt, unsigned int device) {
  // size
  Serial.print(lowByte(LifxPacketSize + pkt.data_size), HEX);
  Serial.print(SPACE);
  Serial.print(highByte(LifxPacketSize + pkt.data_size), HEX);
  Serial.print(SPACE);

  // protocol
  Serial.print(lowByte(pkt.protocol), HEX);
  Serial.print(SPACE);
  Serial.print(highByte(pkt.protocol), HEX);
  Serial.print(SPACE);

  // source
  Serial.print(lowByte(pkt.source[0]), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(pkt.source[1]), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(pkt.source[2]), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(pkt.source[3]), HEX);
  Serial.print(SPACE);

  // target mac[device] address
  for(int i = 0; i < sizeof(mac[device]); i++) {
    Serial.print(lowByte(mac[device][i]), HEX);
    Serial.print(SPACE);
  }

  // reserved2
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);

  // site mac address
  for(int i = 0; i < sizeof(site_mac); i++) {
    Serial.print(lowByte(site_mac[i]), HEX);
    Serial.print(SPACE);
  }

  // reserved3
  Serial.print(lowByte(0x00), HEX);
  Serial.print("S");
  Serial.print(lowByte(pkt.sequence), HEX);
  Serial.print(SPACE);

  // reserved4
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);

  //packet type
  Serial.print(lowByte(pkt.packet_type), HEX);
  Serial.print(SPACE);
  Serial.print(highByte(pkt.packet_type), HEX);
  Serial.print(SPACE);

  // reserved5
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);
  Serial.print(lowByte(0x00), HEX);
  Serial.print(SPACE);

  //data
  for(int i = 0; i < pkt.data_size; i++) {
    Serial.print(pkt.data[i], HEX);
    Serial.print(SPACE);
  }
}

void setLight() {
  if(DEBUG) {
    Serial.print(F("Set light - "));
    Serial.print(F("hue: "));
    Serial.print(hue);
    Serial.print(F(", sat: "));
    Serial.print(sat);
    Serial.print(F(", bri: "));
    Serial.print(bri);
    Serial.print(F(", kel: "));
    Serial.print(kel);
    Serial.print(F(", power: "));
    Serial.print(power_status);
    Serial.println(power_status ? " (on)" : "(off)");
  }

  if(power_status) {
    int this_hue = map(hue, 0, 65535, 0, 359);
    int this_sat = map(sat, 0, 65535, 0, 255);
    int this_bri = map(bri, 0, 65535, 0, 255);

    // if we are setting a "white" colour (kelvin temp)
    if(kel > 0 && this_sat < 1) {
      // convert kelvin to RGB
      rgb kelvin_rgb;
      kelvin_rgb = kelvinToRGB(kel);

      // convert the RGB into HSV
      hsv kelvin_hsv;
      kelvin_hsv = rgb2hsv(kelvin_rgb);

      // set the new values ready to go to the bulb (brightness does not change, just hue and saturation)
      this_hue = kelvin_hsv.h;
      this_sat = map(kelvin_hsv.s*1000, 0, 1000, 0, 255); //multiply the sat by 1000 so we can map the percentage value returned by rgb2hsv
    }

    LIFXBulb.fadeHSB(this_hue, this_sat, this_bri);
  }
  else {
    LIFXBulb.fadeHSB(0, 0, 0);
  }
}
