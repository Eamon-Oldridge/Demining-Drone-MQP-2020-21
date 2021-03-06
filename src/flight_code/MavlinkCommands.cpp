#include "MavlinkCommands.h"

// Serial port for mavlink to use
// Defaults to Serial1, but redefined by the setup_mavlink() call
HardwareSerial* mav_port = &Serial1;

// Command status variable
enum command_status_t command_status;

int32_t cmd_last_sent_time = -1;  // time since last command was sent
MAV_CMD cmd_last_sent_type;       // type of the last command sent
float cmd_last_sent_param;        // parameter of the last command sent
uint16_t cmd_last_ack = 0;       // type of the last command that was acknowledged

void setup_mavlink(HardwareSerial *serial_ptr, uint32_t mav_baud) {
  mav_port = serial_ptr;
  mav_port->begin(mav_baud);
}

// Reads from MAV_PORT until either:
//   1) the serial buffer is empty, or
//   2) a full message is found
// If (1), then the function returns 0. If any bytes remain, they will be stored statically
// If (2), then the function returns 1 and copies the message and status into the locations specified by the pointers
int receive_mavlink(mavlink_message_t *msg_ptr, mavlink_status_t *stat_ptr) {
  char byte_in;
  mavlink_message_t msg;
  mavlink_status_t stat;
  
  while(mav_port->available()) {   // As long as there's serial data, read it
    byte_in = mav_port->read();
    if(mavlink_parse_char(0, byte_in, &msg, &stat)) {   // If that byte completed the message, then handle it
      *msg_ptr = msg;   // Copy the received message into the location specified by msg_pointer
      *stat_ptr = stat; // Same with the status
      return 1;
    }
  }
  return 0;   // If we made it this far, then we didn't get a message this time
}

// Given a pointer to a message, this function converts it to an array of bytes sends it over MAV_PORT
void send_mavlink(mavlink_message_t* msg_ptr) {
  uint8_t buf[BUF_SIZE];  // Memory location for the message
  uint16_t len = mavlink_msg_to_send_buffer(buf, msg_ptr);   // Serialize the message - convert it into an array of bytes
  mav_port->write(buf,len);  // Send the message
}

// Takes a received ACK message and updates the command_status variable
void set_command_status(mavlink_message_t *msg, mavlink_status_t *stat) {
  uint8_t result = mavlink_msg_command_ack_get_result(msg);
  if(result == MAV_RESULT_ACCEPTED) {
      command_status = ACCEPTED;
  }
  else if(result == MAV_RESULT_TEMPORARILY_REJECTED ||
          result == MAV_RESULT_DENIED ||
          result == MAV_RESULT_UNSUPPORTED ||
          result == MAV_RESULT_FAILED) {
    // The Pixhawk returned an error - send something back to the base station
    command_status = REJECTED;
  }
  else if(result == MAV_RESULT_IN_PROGRESS) {
    command_status = IN_PROGRESS;
  }
}

// Arms the drone
void arm(bool resend) {
  mavlink_message_t msg;
  static uint8_t confirmation = 0;
  if(resend == true)
    confirmation++;
  else
    confirmation = 0;
  // Pack a MAV_CMD_COMPONENT_ARM_DISARM command with the arm parameter set, into a command_long message
  mavlink_msg_command_long_pack(SYS_ID, COMP_ID, &msg, TARGET_SYS, TARGET_COMP, MAV_CMD_COMPONENT_ARM_DISARM, confirmation, ARM_CONDITION,0,0,0,0,0,0); // ARM
  send_mavlink(&msg);
  cmd_last_sent_time = millis();
  cmd_last_sent_type = MAV_CMD_COMPONENT_ARM_DISARM;
  cmd_last_sent_param = ARM_CONDITION;
  command_status = SENT;
}

// Disarm the drone
void disarm(bool resend) {
  mavlink_message_t msg;
  static uint8_t confirmation = 0;
  if(resend == true)
    confirmation++;
  else
    confirmation = 0;
  // Pack a MAV_CMD_COMPONENT_ARM_DISARM command with the arm parameter cleared, into a command_long message
  mavlink_msg_command_long_pack(SYS_ID, COMP_ID, &msg, TARGET_SYS, TARGET_COMP, MAV_CMD_COMPONENT_ARM_DISARM, confirmation, DISARM_CONDITION,0,0,0,0,0,0); // DISARM
  send_mavlink(&msg);
  cmd_last_sent_time = millis();
  cmd_last_sent_type = MAV_CMD_COMPONENT_ARM_DISARM;
  cmd_last_sent_param = DISARM_CONDITION;
  command_status = SENT;
}

// Takeoff
// Takes off and climbs to OPERATING_ALT
// Maintains lat/lon position
void takeoff(bool resend) {
  mavlink_message_t msg;
  static uint8_t confirmation = 0;
  if(resend == true)
    confirmation++;
  else
    confirmation = 0;
  // Pack a MAV_CMD_NAV_TAKEOFF command with the altitude parameter set, into a command_long message
  mavlink_msg_command_long_pack(SYS_ID, COMP_ID, &msg, TARGET_SYS, TARGET_COMP, MAV_CMD_NAV_TAKEOFF, confirmation, 0,0,0,0,0,0,OPERATING_ALT); // TAKEOFF
  send_mavlink(&msg);
  cmd_last_sent_time = millis();
  cmd_last_sent_type = MAV_CMD_NAV_TAKEOFF;
  command_status = SENT;
}

// Sets a target position for the drone to fly to
void set_position_target(int32_t target_lat, int32_t target_lon) {
  mavlink_message_t msg;
  mavlink_msg_set_position_target_global_int_pack(SYS_ID, COMP_ID, &msg, millis(), TARGET_SYS, TARGET_COMP, MAV_FRAME_GLOBAL_RELATIVE_ALT_INT, SET_POS_TYPE_MASK, target_lat, target_lon, OPERATING_ALT, 0,0,0, 0,0,0, 0,0);
  send_mavlink(&msg);
}

// Send to drone back to the takeoff point and land there
void return_to_launch(bool resend) {
  mavlink_message_t msg;
  static uint8_t confirmation = 0;
  if(resend == true)
    confirmation++;
  else
    confirmation = 0;
  mavlink_msg_command_long_pack(SYS_ID, COMP_ID, &msg, TARGET_SYS, TARGET_COMP, MAV_CMD_NAV_RETURN_TO_LAUNCH, confirmation, 0,0,0,0,0,0,0); // RTL
  send_mavlink(&msg);
  cmd_last_sent_time = millis();
  cmd_last_sent_type = MAV_CMD_NAV_RETURN_TO_LAUNCH;
  command_status = SENT;
}

// Sends a message to the flight controller telling it to send a position estimate every 1 second
void initiate_GPS_data(bool resend) {
  mavlink_message_t msg;
  static uint8_t confirmation = 0;
  if(resend == true)
    confirmation++;
  else
    confirmation = 0;
  mavlink_msg_command_long_pack(SYS_ID, COMP_ID, &msg, TARGET_SYS, TARGET_COMP, MAV_CMD_SET_MESSAGE_INTERVAL, confirmation, MAVLINK_MSG_ID_GLOBAL_POSITION_INT, 1000000,0,0,0,0,0);  // request GLOBAL_POSITION_INT at 1 Hz
  send_mavlink(&msg);
  cmd_last_sent_time = millis();
  cmd_last_sent_type = MAV_CMD_SET_MESSAGE_INTERVAL;
  cmd_last_sent_param = MAVLINK_MSG_ID_GLOBAL_POSITION_INT;
  command_status = SENT;
}

// Sends a message to the flight controller telling it to send the raw GPS data every 1 second
void initiate_GPS_fix_data(bool resend) {
  mavlink_message_t msg;
  static uint8_t confirmation = 0;
  if(resend == true)
    confirmation++;
  else
    confirmation = 0;
  mavlink_msg_command_long_pack(SYS_ID, COMP_ID, &msg, TARGET_SYS, TARGET_COMP, MAV_CMD_SET_MESSAGE_INTERVAL, confirmation, MAVLINK_MSG_ID_GPS2_RAW, 1000000,0,0,0,0,0);  // request GLOBAL_POSITION_INT at 1 Hz
  send_mavlink(&msg);
  cmd_last_sent_time = millis();
  cmd_last_sent_type = MAV_CMD_SET_MESSAGE_INTERVAL;
  cmd_last_sent_param = MAVLINK_MSG_ID_GPS_RAW_INT;
  command_status = SENT;
}

// Sends a message to the flight controller telling it to send back the home position of the drone
void request_home_location(bool resend) {
  mavlink_message_t msg;
  static uint8_t confirmation = 0;
  if(resend == true)
    confirmation++;
  else
    confirmation = 0;
  mavlink_msg_command_long_pack(SYS_ID, COMP_ID, &msg, TARGET_SYS, TARGET_COMP, MAV_CMD_GET_HOME_POSITION, confirmation, 0,0,0,0,0,0,0);  // request home position
  send_mavlink(&msg);
  cmd_last_sent_time = millis();
  cmd_last_sent_type = MAV_CMD_GET_HOME_POSITION;
  command_status = SENT;
}

// Sends a message to the flight controller telling it to update the home location to the present position
void reset_home_location(bool resend) {
  mavlink_message_t msg;
  static uint8_t confirmation = 0;
  if(resend == true)
    confirmation++;
  else
    confirmation = 0;
  mavlink_msg_command_long_pack(SYS_ID, COMP_ID, &msg, TARGET_SYS, TARGET_COMP, MAV_CMD_DO_SET_HOME, confirmation, 1,0,0,0,0,0,0); // Set to current location
  send_mavlink(&msg);
  cmd_last_sent_time = millis();
  cmd_last_sent_type = MAV_CMD_DO_SET_HOME;
  command_status = SENT;
}

// returns false if no action was taken, or true if a message was resent
bool check_timeouts() {
  int32_t cur_time = millis();
  if(command_status == SENT) {
    if(cur_time - cmd_last_sent_time >= NO_ACK_TIMEOUT) {
      resend();  // if the command has not been acknowledged, resend it
      return true;
    }
  }
  return false;
}

void resend() {
  switch(cmd_last_sent_type) {
    case MAV_CMD_NAV_TAKEOFF:
      takeoff(true);
      break;
    case MAV_CMD_COMPONENT_ARM_DISARM:
      if(cmd_last_sent_param == ARM_CONDITION) {
        arm(true);
      }
      else if(cmd_last_sent_param == DISARM_CONDITION) {
        disarm(true);
      }
      break;
    case MAV_CMD_NAV_RETURN_TO_LAUNCH:
      return_to_launch(true);
      break;
    case MAV_CMD_SET_MESSAGE_INTERVAL:
      if(cmd_last_sent_param == MAVLINK_MSG_ID_GLOBAL_POSITION_INT) {
        initiate_GPS_data(true);
      }
      break;
    case MAV_CMD_GET_HOME_POSITION:
      request_home_location(true);
      break;
    case MAV_CMD_DO_SET_HOME:
      reset_home_location(true);
      break;
    default:
      break;
  }
}
