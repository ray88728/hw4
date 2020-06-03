#include "mbed.h"

#include "MQTTNetwork.h"

#include "MQTTmbed.h"

#include "MQTTClient.h"

#include "fsl_port.h"

#include "fsl_gpio.h"

#include "mbed_rpc.h"

#define UINT14_MAX        16383

// FXOS8700CQ I2C address

#define FXOS8700CQ_SLAVE_ADDR0 (0x1E<<1) // with pins SA0=0, SA1=0

#define FXOS8700CQ_SLAVE_ADDR1 (0x1D<<1) // with pins SA0=1, SA1=0

#define FXOS8700CQ_SLAVE_ADDR2 (0x1C<<1) // with pins SA0=0, SA1=1

#define FXOS8700CQ_SLAVE_ADDR3 (0x1F<<1) // with pins SA0=1, SA1=1

// FXOS8700CQ internal register addresses

#define FXOS8700Q_STATUS 0x00

#define FXOS8700Q_OUT_X_MSB 0x01

#define FXOS8700Q_OUT_Y_MSB 0x03

#define FXOS8700Q_OUT_Z_MSB 0x05

#define FXOS8700Q_M_OUT_X_MSB 0x33

#define FXOS8700Q_M_OUT_Y_MSB 0x35

#define FXOS8700Q_M_OUT_Z_MSB 0x37

#define FXOS8700Q_WHOAMI 0x0D

#define FXOS8700Q_XYZ_DATA_CFG 0x0E

#define FXOS8700Q_CTRL_REG1 0x2A

#define FXOS8700Q_M_CTRL_REG1 0x5B

#define FXOS8700Q_M_CTRL_REG2 0x5C

#define FXOS8700Q_WHOAMI_VAL 0xC7



// GLOBAL VARIABLES

WiFiInterface *wifi;

volatile int message_num = 0;

volatile int arrivedcount = 0;

volatile bool closed = false;


int change=0;

const char* topic = "Mbed";

int tilt =0;

int sample_time=0;

float t[3];

int sum_data =0;

char temp[100];

I2C i2c( PTD9,PTD8);

int fast =0;

int m_addr = FXOS8700CQ_SLAVE_ADDR1;

void FXOS8700CQ_readRegs(int addr, uint8_t * data, int len);

void FXOS8700CQ_writeRegs(uint8_t * data, int len);

void getAcc(Arguments *in, Reply *out);

Thread mqtt_thread;

EventQueue mqtt_queue;

RPCFunction rpcAcc(&getAcc, "getAcc");

RawSerial pc(USBTX, USBRX);

RawSerial xbee(D12, D11);


EventQueue queue(32 * EVENTS_EVENT_SIZE);

DigitalOut redLED(LED1);

Thread thr;

void xbee_rx_interrupt(void);

void xbee_rx(void);

void reply_messange(char *xbee_reply, char *messange);

void check_addr(char *xbee_reply, char *messenger);

void messageArrived(MQTT::MessageData& md) {

      MQTT::Message &message = md.message;

      char msg[300];

      sprintf(msg, "Message arrived: QoS%d, retained %d, dup %d, packetID %d\r\n", message.qos, message.retained, message.dup, message.id);

      printf(msg);

      wait_ms(1000);

      char payload[300];

      sprintf(payload, "Payload %.*s\r\n", message.payloadlen, (char*)message.payload);

      printf(payload);

      ++arrivedcount;

}


void publish_message(MQTT::Client<MQTTNetwork, Countdown>* client) {

      message_num++;

      MQTT::Message message;

      char buff[100];

      if(t[0]>=0&&t[1]>=0&&t[2]>=0)
         sprintf(buff, "+%l.4f+%l.4f+%l.4f", t[0],t[1],t[2]);

      if(t[0]>=0&&t[1]>=0&&t[2]<0)
         sprintf(buff, "+%l.4f+%l.4f%l.4f", t[0],t[1],t[2]);

      if(t[0]>=0&&t[1]<0&&t[2]>=0)
         sprintf(buff, "+%l.4f%l.4f+%l.4f", t[0],t[1],t[2]);

      if(t[0]>=0&&t[1]<0&&t[2]<0)
         sprintf(buff, "+%l.4f%l.4f%l.4f", t[0],t[1],t[2]);

      if(t[0]<0&&t[1]>=0&&t[2]>=0)
         sprintf(buff, "%l.4f+%l.4f+%l.4f", t[0],t[1],t[2]);

      if(t[0]<0&&t[1]>=0&&t[2]<0)
         sprintf(buff, "%l.4f+%l.4f%l.4f", t[0],t[1],t[2]);
         
      if(t[0]<0&&t[1]<0&&t[2]>=0)
         sprintf(buff, "%l.4f%l.4f+%l.4f", t[0],t[1],t[2]);

      if(t[0]<0&&t[1]<0&&t[2]<0)
         sprintf(buff, "%l.4f%l.4f%l.4f", t[0],t[1],t[2]);

      message.qos = MQTT::QOS0;

      message.retained = false;

      message.dup = false;

      message.payload = (void*) buff;

      message.payloadlen = strlen(buff) + 1;

      int rc = client->publish(topic, message);


      pc.printf("rc:  %d\r\n", rc);

      printf("%s\n", buff);

}


int main() {


      char xbee_reply[4];

      redLED =1;

      wifi = WiFiInterface::get_default_instance();

      if (!wifi) {

            printf("ERROR: No WiFiInterface found.\r\n");

            return -1;

      }



      printf("\nConnecting to %s...\r\n", MBED_CONF_APP_WIFI_SSID);

      int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);

      if (ret != 0) {

            printf("\nConnection error: %d\r\n", ret);

            return -1;

      }



      NetworkInterface* net = wifi;

      MQTTNetwork mqttNetwork(net);

      MQTT::Client<MQTTNetwork, Countdown> client(mqttNetwork);


      //TODO: revise host to your ip

      const char* host = "192.168.43.151";

      printf("Connecting to TCP network...\r\n");

      int rc = mqttNetwork.connect(host, 1883);

      if (rc != 0) {

            printf("Connection error.");

            return -1;

      }

      printf("Successfully connected!\r\n");


      MQTTPacket_connectData data = MQTTPacket_connectData_initializer;

      data.MQTTVersion = 3;

      data.clientID.cstring = "Mbed";


      if ((rc = client.connect(data)) != 0){

            printf("Fail to connect MQTT\r\n");

      }

      if (client.subscribe(topic, MQTT::QOS0, messageArrived) != 0){

            printf("Fail to subscribe\r\n");

      }




   //   btn2.rise(mqtt_queue.event(&publish_message, &client));


//      printf("1\n");

      int num = 0;
      
      pc.baud(9600);

      xbee.baud(9600);

      xbee.printf("+++");

      xbee_reply[0] = xbee.getc();

      xbee_reply[1] = xbee.getc();

      if(xbee_reply[0] == 'O' && xbee_reply[1] == 'K'){

         pc.printf("enter AT mode.\r\n");

         xbee_reply[0] = '\0';

         xbee_reply[1] = '\0';

      }

      xbee.printf("ATMY 0x240\r\n");

      reply_messange(xbee_reply, "setting MY : 0x240");

      xbee.printf("ATDL 0x0\r\n");

      reply_messange(xbee_reply, "setting DL : 0x0");

      xbee.printf("ATID 0x4041\r\n");

      reply_messange(xbee_reply, "setting PAN ID : 0x4041");

      xbee.printf("ATWR\r\n");

      reply_messange(xbee_reply, "write config");

      //xbee.printf("ATMY\r\n");

    //  check_addr(xbee_reply, "MY");

  //    xbee.printf("ATDL\r\n");

//      check_addr(xbee_reply, "DL");

      xbee.printf("ATCN\r\n");

      reply_messange(xbee_reply, "exit AT mode");

      xbee.getc();


      // start


      // Setup a serial interrupt function of receiving data from xbee

 



      uint8_t who_am_i, dataa[2], res[6];

      int16_t acc16;  

      FXOS8700CQ_readRegs( FXOS8700Q_CTRL_REG1, &dataa[1], 1);

      dataa[1] |= 0x01;

      dataa[0] = FXOS8700Q_CTRL_REG1;

      FXOS8700CQ_writeRegs(dataa, 2);

      // Get the slave address

      FXOS8700CQ_readRegs(FXOS8700Q_WHOAMI, &who_am_i, 1);

      mqtt_thread.start(callback(&mqtt_queue, &EventQueue::dispatch_forever));

      thr.start(callback(&queue, &EventQueue::dispatch_forever));

    //  xbee.attach(xbee_rx_interrupt, Serial::RxIrq);   
      queue.call(&xbee_rx);

      while (1) {

         FXOS8700CQ_readRegs(FXOS8700Q_OUT_X_MSB, res, 6);

         acc16 = (res[0] << 6) | (res[1] >> 2);

         if (acc16 > UINT14_MAX/2)

            acc16 -= UINT14_MAX;

         t[0] = ((float)acc16) / 4096.0f;

         acc16 = (res[2] << 6) | (res[3] >> 2);

         if (acc16 > UINT14_MAX/2)

            acc16 -= UINT14_MAX;

         t[1] = ((float)acc16) / 4096.0f;


         acc16 = (res[4] << 6) | (res[5] >> 2);

         if (acc16 > UINT14_MAX/2)

            acc16 -= UINT14_MAX;

         t[2] = ((float)acc16) / 4096.0f;

         mqtt_queue.call(&publish_message, &client);

         if( ((t[2] < 1/sqrt(2))||sample_time!=0)&&change ==0 ){
            if(sample_time < 10){
               sample_time = sample_time + 1;

            }
            else
            {
               change =1;
               sample_time =0;
            }
          //  sum_data = sum_data +1;   
            wait(0.1);
         }
         else
         {
            change =0;
          //  sum_data = sum_data +1;   
            wait(0.5);
         }

         sum_data=sum_data+1;

      }



      printf("Ready to close MQTT Network......\n");


      if ((rc = client.unsubscribe(topic)) != 0) {

            printf("Failed: rc from unsubscribe was %d\n", rc);

      }

      if ((rc = client.disconnect()) != 0) {

      printf("Failed: rc from disconnect was %d\n", rc);

      }


      mqttNetwork.disconnect();

      printf("Successfully closed!\n");


      return 0;

}

void FXOS8700CQ_readRegs(int addr, uint8_t * data, int len) {

   char t = addr;

   i2c.write(m_addr, &t, 1, true);

   i2c.read(m_addr, (char *)data, len);

}


void FXOS8700CQ_writeRegs(uint8_t * data, int len) {

   i2c.write(m_addr, (char *)data, len);

}


void xbee_rx_interrupt(void)

{

  xbee.attach(NULL, Serial::RxIrq); // detach interrupt

  queue.call(&xbee_rx);

}


void xbee_rx(void)

{
while(1){
  char buf[30] = {0};
  char outbuf[30] = {0};
  while(xbee.readable()){
    for (int i=0; ; i++) {
      char recv = xbee.getc();
      if (recv == '\r') {
        break;
      }
      buf[i] = pc.putc(recv);
    }
    //fast =1;
    RPC::call(buf, outbuf);
    //pc.printf("%s\r\n", outbuf);
    //fast =0;
    wait(0.00005);
  }
}
//redLED=1;
//  xbee.attach(xbee_rx_interrupt, Serial::RxIrq); // reattach interrupt

}


void reply_messange(char *xbee_reply, char *messange){

  xbee_reply[0] = xbee.getc();

  xbee_reply[1] = xbee.getc();

  xbee_reply[2] = xbee.getc();

  if(xbee_reply[1] == 'O' && xbee_reply[2] == 'K'){

    pc.printf("%s\r\n", messange);

    xbee_reply[0] = '\0';

    xbee_reply[1] = '\0';

    xbee_reply[2] = '\0';

  }

}


void check_addr(char *xbee_reply, char *messenger){

  xbee_reply[0] = xbee.getc();

  xbee_reply[1] = xbee.getc();

  xbee_reply[2] = xbee.getc();

  xbee_reply[3] = xbee.getc();

  pc.printf("%s = %c%c%c\r\n", messenger, xbee_reply[1], xbee_reply[2], xbee_reply[3]);

  xbee_reply[0] = '\0';

  xbee_reply[1] = '\0';

  xbee_reply[2] = '\0';

  xbee_reply[3] = '\0';

}

void getAcc(Arguments *in, Reply *out) {

    xbee.printf("%d\r\n", sum_data/10);
    wait(0.00003);
    xbee.printf("%d\r\n", sum_data%10);
    sum_data =0;
   // redLED=0;
}
