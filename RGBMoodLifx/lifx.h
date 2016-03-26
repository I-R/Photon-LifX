
// Added in for Particle
typedef uint8_t byte;
typedef uint16_t word;
#define lowByte(w) ((uint8_t) ((w) & 0xff))
#define highByte(w) ((uint8_t) ((w) >> 8))
#define word(b1,b2)  (uint16_t(uint8_t(b1)<<8|uint8_t(b2)))

struct LifxPacket {
  /* FRAME */
  /* size */
  uint16_t size; //little endian
  /*
    origin[2] : Message origin indicator - must be 0,
    tagged[1] : Broadcast (0) or unicast (1) - set target/bubAddress,
    addressable[1] : Message includes a target address: must be 1
    protocol[12] - must be 1024
    */
  uint16_t protocol; //little endian
  /*
    source[32] : Unique identifier, set by client, used by response
    */
  byte source[4];

  /* FRAME Address */
  /* target[64]: 6-byte MAC, left padded or zero(0)-> all devices */
  byte target[6];
  uint16_t reserved2;
  /* reserved[48] - must be zero */
  byte site[6];
  /* reserved[6]
  ack_required[1] : bool - ack required
  res_reqiored[1] : bool - response required
  sequence[8]     : byte - wrap around message sequence number
  */
  byte reserved3;
  byte sequence;
  /* PROTOCOL HEADER */
  /* reserved[64] : reserved - must be 0*/
  byte reserved4[8];
  /* type[16]     : Message type */
  int16_t packet_type; //little endian
  /* reserved[16] */
  uint16_t reserved5;

  byte data[128];
  int data_size;
};

const unsigned int LifxProtocol_AllBulbsResponse = 21504; // 0x5400
const unsigned int LifxProtocol_AllBulbsRequest  = 13312; // 0x3400
const unsigned int LifxProtocol_BulbCommand      = 5120;  // 0x1400

const unsigned int LifxPacketSize      = 36;
const unsigned int LifxPort            = 56700;  // local port to listen on
const unsigned int LifxBulbLabelLength = 32;
const unsigned int LifxBulbTagsLength = 8;
const unsigned int LifxBulbTagLabelsLength = 32;

// firmware versions, etc
const unsigned int LifxBulbVendor = 1;
const unsigned int LifxBulbProduct = 1;
const unsigned int LifxBulbVersion = 1;
const unsigned int LifxFirmwareVersionMajor = 1;
const unsigned int LifxFirmwareVersionMinor = 5;

const byte SERVICE_UDP = 0x01;
const byte SERVICE_TCP = 0x02;

// packet types
const byte GET_PAN_GATEWAY = 0x02;
const byte PAN_GATEWAY = 0x03;

const byte GET_WIFI_FIRMWARE_STATE = 0x12;
const byte WIFI_FIRMWARE_STATE = 0x13;

const byte GET_POWER_STATE = 0x14;
const byte SET_POWER_STATE = 0x75;
const byte POWER_STATE = 0x16;

const byte GET_BULB_LABEL = 0x17;
const byte SET_BULB_LABEL = 0x18;
const byte BULB_LABEL = 0x19;

const byte GET_VERSION_STATE = 0x20;
const byte VERSION_STATE = 0x21;

const byte GET_BULB_TAGS = 0x1a;
const byte SET_BULB_TAGS = 0x1b;
const byte BULB_TAGS = 0x1c;

const byte GET_BULB_TAG_LABELS = 0x1d;
const byte SET_BULB_TAG_LABELS = 0x1e;
const byte BULB_TAG_LABELS = 0x1f;

const byte GET_LIGHT_STATE = 0x65;
const byte SET_LIGHT_STATE = 0x66;
const byte LIGHT_STATUS = 0x6b;

const byte GET_MESH_FIRMWARE_STATE = 0x0e;
const byte MESH_FIRMWARE_STATE = 0x0f;


#define EEPROM_BULB_LABEL_START 0 // 32 bytes long
#define EEPROM_BULB_TAGS_START 32 // 8 bytes long
#define EEPROM_BULB_TAG_LABELS_START 40 // 32 bytes long
// future data for EEPROM will start at 72...
#define EEPROM_BULB_CONFIG 72 // One config Block takes 72 Bytes

#define EEPROM_CONFIG "PL1" // 3 byte identifier for this sketch's EEPROM settings
#define EEPROM_CONFIG_START 253 // store EEPROM_CONFIG at the end of EEPROM

// helpers
#define SPACE " "
