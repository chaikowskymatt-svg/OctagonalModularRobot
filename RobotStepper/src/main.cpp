#include <Arduino.h>
#include <micro_ros_platformio.h>
#include <WiFi.h>
#include <rcl/rcl.h>
#include <rclc/rclc.h>
#include <rclc/executor.h>
#include <std_msgs/msg/string.h>

// ── Change this to 2 when flashing robot 2 ───────────────
#define ROBOT_ID 2

char WIFI_SSID[]     = "USER_NAME";//Add UserName
char WIFI_PASSWORD[] = "PASSWORD"; //Add Password
IPAddress AGENT_IP(10, 183, 30, 141);
const uint16_t AGENT_PORT = 8888;

#define M1_STEP 18
#define M1_DIR  19
#define M2_STEP 22
#define M2_DIR  23

#define STEP_DELAY_US 1500
#define HOME_STEPS    800

volatile bool stop_flag = false;

void step_both(int s1, int s2, int delay_us = STEP_DELAY_US) {
  if (s1 == 0 && s2 == 0) return;
  digitalWrite(M1_DIR, s1 >= 0 ? HIGH : LOW);
  digitalWrite(M2_DIR, s2 >= 0 ? HIGH : LOW);
  delayMicroseconds(10);
  int c1 = abs(s1), c2 = abs(s2), total = max(c1, c2);
  for (int i = 0; i < total; i++) {
    if (stop_flag) { stop_flag = false; return; }
    if (i < c1) digitalWrite(M1_STEP, HIGH);
    if (i < c2) digitalWrite(M2_STEP, HIGH);
    delayMicroseconds(5);
    digitalWrite(M1_STEP, LOW);
    digitalWrite(M2_STEP, LOW);
    delayMicroseconds(delay_us - 5);
  }
}

void handle_cmd(const char* cmd) {
  Serial.printf("CMD: [%s]\n", cmd);

  if (strcmp(cmd, "stop") == 0) { stop_flag = true; return; }
  stop_flag = false;

  if (strcmp(cmd, "home") == 0) {
    step_both(-HOME_STEPS, -HOME_STEPS, 2000); return;
  }
  if (strncmp(cmd, "jog:", 4) == 0) {
    // format: jog:m1:400  or  jog:m2:-400
    int motor = 0, steps = 0;
    sscanf(cmd, "jog:m%d:%d", &motor, &steps);
    Serial.printf("JOG m%d %d steps\n", motor, steps);
    if (motor == 1) step_both(steps, 0);
    else            step_both(0, steps);
    return;
  }
  if (strncmp(cmd, "both:", 5) == 0) {
    int steps = atoi(cmd + 5);
    Serial.printf("BOTH %d\n", steps);
    step_both(steps, steps);
    return;
  }
  if (strncmp(cmd, "seq:", 4) == 0) {
    const char* p = cmd + 4;
    int cycles = atoi(p);
    if (cycles < 1) cycles = 1;
    while (*p && *p != ':') p++;
    if (*p == ':') p++;
    Serial.printf("SEQ cycles=%d\n", cycles);
    for (int c = 0; c < cycles; c++) {
      const char* s = p;
      while (*s) {
        if (stop_flag) { stop_flag = false; return; }
        int m1 = atoi(s);
        while (*s && *s != ',') s++;
        if (*s == ',') s++;
        int m2 = atoi(s);
        while (*s && *s != ',') s++;
        if (*s == ',') s++;
        int pause = atoi(s);
        while (*s && *s != '|') s++;
        if (*s == '|') s++;
        Serial.printf("  m1=%d m2=%d pause=%d\n", m1, m2, pause);
        step_both(m1, m2);
        delay(pause);
      }
    }
    Serial.println("SEQ done.");
    return;
  }
  Serial.println("Unknown cmd.");
}

rcl_allocator_t    allocator;
rclc_support_t     support;
rcl_node_t         node;
rclc_executor_t    executor;
rcl_subscription_t sub_cmd;
std_msgs__msg__String cmd_msg;
char topic_name[32];
char node_name[32];

void cmd_callback(const void* msg_in) {
  handle_cmd(((const std_msgs__msg__String*)msg_in)->data.data);
}

void setup() {
  Serial.begin(115200);
  delay(500);
  snprintf(node_name,  sizeof(node_name),  "robot%d_node",  ROBOT_ID);
  snprintf(topic_name, sizeof(topic_name), "/robot%d/cmd",  ROBOT_ID);
  Serial.printf("Booting %s...\n", node_name);

  pinMode(M1_STEP,OUTPUT); pinMode(M1_DIR,OUTPUT);
  pinMode(M2_STEP,OUTPUT); pinMode(M2_DIR,OUTPUT);
  digitalWrite(M1_STEP,LOW); digitalWrite(M1_DIR,LOW);
  digitalWrite(M2_STEP,LOW); digitalWrite(M2_DIR,LOW);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500); Serial.print("."); attempts++;
  }
  Serial.printf("\nWiFi: %s\n", WiFi.localIP().toString().c_str());

  set_microros_wifi_transports(WIFI_SSID, WIFI_PASSWORD, AGENT_IP, AGENT_PORT);
  delay(2000);

  allocator = rcl_get_default_allocator();
  rclc_support_init(&support, 0, NULL, &allocator);
  rclc_node_init_default(&node, node_name, "", &support);
  rclc_subscription_init_default(&sub_cmd, &node,
    ROSIDL_GET_MSG_TYPE_SUPPORT(std_msgs, msg, String), topic_name);
  cmd_msg.data.data     = (char*)malloc(512);
  cmd_msg.data.size     = 0;
  cmd_msg.data.capacity = 512;
  rclc_executor_init(&executor, &support.context, 1, &allocator);
  rclc_executor_add_subscription(&executor, &sub_cmd, &cmd_msg, &cmd_callback, ON_NEW_DATA);
  Serial.printf("%s ready on %s\n", node_name, topic_name);
}

void loop() {
  rclc_executor_spin_some(&executor, RCL_MS_TO_NS(10));
  delay(10);
}
