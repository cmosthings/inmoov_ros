/*
 * rosserial Servo Control Example
 *
 * This sketch demonstrates the control of hobby R/C servos
 * using ROS and the arduiono
 * 
 * For the full tutorial write up, visit
 * www.ros.org/wiki/rosserial_arduino_demos
 *
 * For more information on the Arduino Servo Library
 * Checkout :
 * http://www.arduino.cc/en/Reference/Servo
 */

//#if (ARDUINO >= 100)
 #include <Arduino.h>
//#else
// #include <WProgram.h>
//#endif

#include <Servo.h> 
#include <ros.h>
#include <sensor_msgs/JointState.h>

ros::NodeHandle  nh;

Servo servo;

void servo_cb(const sensor_msgs::JointState& cmd_msg){
  servo.write((int)((cmd_msg.position[15])/180*3.1415)); //set servo angle, should be from 0-180  
  digitalWrite(13, HIGH-digitalRead(13));  //toggle led #

}



ros::Subscriber<sensor_msgs::JointState> sub("joint_states", servo_cb);

void setup(){
  pinMode(13, OUTPUT);

  nh.initNode();
  nh.subscribe(sub);
  
  servo.attach(3); //attach it to pin 9
}

void loop(){
  nh.spinOnce();
  delay(1);
}