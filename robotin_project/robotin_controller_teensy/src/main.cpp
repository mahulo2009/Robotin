#include <Arduino.h>

#include <ros.h>
#include <geometry_msgs/Twist.h>
#include <robotin_project/RAW_VEL.h>
#include <robotin_project/PID.h>
#include <robotin_project/TEL.h>
#include <Encoder.h>
#include <PID.h>

//----------------------------------Configuration
#define COMMAND_RATE 20 //hz

double wheel_circumference_ = 0.034*2*M_PI;
double wheels_x_distance_ = 0.10;
double wheels_y_distance_ = 0.215;
double max_kinematic_rpm_ =  215;
int total_wheels_ = 4;

#define MOTOR1_ENCODER_A 14
#define MOTOR1_ENCODER_B 15
#define MOTOR2_ENCODER_A 21
#define MOTOR2_ENCODER_B 20
#define MOTOR3_ENCODER_A 16
#define MOTOR3_ENCODER_B 17
#define MOTOR4_ENCODER_A 23
#define MOTOR4_ENCODER_B 22

#define MOTOR1_PWM  6
#define MOTOR1_IN_A 9
#define MOTOR1_IN_B 8
#define MOTOR2_PWM  5 
#define MOTOR2_IN_A 2
#define MOTOR2_IN_B 7
#define MOTOR3_PWM  3
#define MOTOR3_IN_A 0
#define MOTOR3_IN_B 1
#define MOTOR4_PWM  4
#define MOTOR4_IN_A 11
#define MOTOR4_IN_B 10

#define K_P 0.8 // P constant
#define K_I 0.1 // I constant
#define K_D 0.5 // D constant
#define PWM_MIN -255
#define PWM_MAX 255

//----------------------------------Encoder
Encoder motor1_encoder(MOTOR1_ENCODER_A, MOTOR1_ENCODER_B, 1200);
Encoder motor2_encoder(MOTOR2_ENCODER_A, MOTOR2_ENCODER_B, 1200);
Encoder motor3_encoder(MOTOR3_ENCODER_A, MOTOR3_ENCODER_B, 1200);
Encoder motor4_encoder(MOTOR4_ENCODER_A, MOTOR4_ENCODER_B, 1200);

//----------------------------------Pid
PID motor1_pid(PWM_MIN, PWM_MAX, K_P, K_I, K_D);
PID motor2_pid(PWM_MIN, PWM_MAX, K_P, K_I, K_D);
PID motor3_pid(PWM_MIN, PWM_MAX, K_P, K_I, K_D);
PID motor4_pid(PWM_MIN, PWM_MAX, K_P, K_I, K_D);

//-----------------------------Kinematic
struct kinematic_rpm {
  int motor1;
  int motor2;
  int motor3;
  int motor4;
};

struct kinematic_velocities {
  float linear_x;
  float linear_y;
  float angular_z;
};

kinematic_rpm kinematic_rpm_target;
kinematic_rpm kinematic_rpm_current;
kinematic_rpm kinematic_rpm_demanded;

kinematic_rpm kinematic_direct(const kinematic_velocities &kinematic_velocities_t);
kinematic_velocities kinematic_inverse(const kinematic_rpm &kinematic_rpm_t);

//-----------------------------Motor Drive
void motor_drive_init(int motor_pinA_,int motor_pinB_,int pwm_pin_);
void motor_drive_move(int pwm,int motor_pinA_,int motor_pinB_,int pwm_pin_);

//-----------------------------Robot base
kinematic_velocities kinematic_velocities_t;

kinematic_velocities robot_base_move();
void robot_base_stop();
//-----------------------------ROS
ros::NodeHandle ros_nh;

unsigned long ros_prev_command_time = 0;
robotin_project::RAW_VEL ros_raw_vel_msg;
robotin_project::TEL ros_pid1_tel_msg;
robotin_project::TEL ros_pid2_tel_msg;
robotin_project::TEL ros_pid3_tel_msg;
robotin_project::TEL ros_pid4_tel_msg;

void ros_publish_velocities(kinematic_velocities kinematic_velocities_t);
void ros_cmd_callback(const geometry_msgs::Twist & cmd_msg);
void ros_pid_callback(const robotin_project::PID& pid);

ros::Subscriber<geometry_msgs::Twist> ros_cmd_sub("cmd_vel",ros_cmd_callback);
ros::Subscriber<robotin_project::PID> ros_pid_sub("pid", ros_pid_callback);

ros::Publisher ros_raw_vel_pub("raw_vel", &ros_raw_vel_msg);

ros::Publisher ros_pid1_tel_pub("/pid1/tel_vel",&ros_pid1_tel_msg);
ros::Publisher ros_pid2_tel_pub("/pid2/tel_vel",&ros_pid2_tel_msg);
ros::Publisher ros_pid3_tel_pub("/pid3/tel_vel",&ros_pid3_tel_msg);
ros::Publisher ros_pid4_tel_pub("/pid4/tel_vel",&ros_pid4_tel_msg);

//-----------------------------Setup
void setup() {

  motor_drive_init(MOTOR1_IN_A,MOTOR1_IN_B,MOTOR1_PWM);
  motor_drive_init(MOTOR2_IN_A,MOTOR2_IN_B,MOTOR2_PWM);
  motor_drive_init(MOTOR3_IN_A,MOTOR3_IN_B,MOTOR3_PWM);
  motor_drive_init(MOTOR4_IN_A,MOTOR4_IN_B,MOTOR4_PWM);
  
  ros_nh.initNode();
  ros_nh.subscribe(ros_cmd_sub);
  ros_nh.subscribe(ros_pid_sub);
  ros_nh.advertise(ros_raw_vel_pub);
  ros_nh.advertise(ros_pid1_tel_pub);
  ros_nh.advertise(ros_pid2_tel_pub);
  ros_nh.advertise(ros_pid3_tel_pub);
  ros_nh.advertise(ros_pid4_tel_pub);

  ros_nh.getHardware()->setBaud(115200);

  while(!ros_nh.connected()) {
    ros_nh.spinOnce();
  }

  ros_nh.loginfo("Robotin Connected!");
  delay(1);
}

//-----------------------------Loop
void loop() { 
  static unsigned long prev_control_time = 0; 
  
  if ((millis() - prev_control_time) >= (1000 / COMMAND_RATE)) {
    kinematic_velocities kinematic_velocities_t = robot_base_move();
    prev_control_time = millis();

    ros_publish_velocities(kinematic_velocities_t);
  }

  if ((millis() - ros_prev_command_time) >= 800) {
        robot_base_stop();
  }

  ros_nh.spinOnce();
}

//-------------------ROS
void ros_publish_velocities(kinematic_velocities kinematic_velocities_t) {
  //pass kinematic_velocities to publisher object
  ros_raw_vel_msg.linear_x = kinematic_velocities_t.linear_x;
  ros_raw_vel_msg.linear_y = kinematic_velocities_t.linear_y;
  ros_raw_vel_msg.angular_z = kinematic_velocities_t.angular_z;

    //publish ros_raw_vel_msg
  ros_raw_vel_pub.publish(&ros_raw_vel_msg);

  ros_pid1_tel_msg.target_velocity=kinematic_rpm_target.motor1;
  ros_pid1_tel_msg.current_velocity=kinematic_rpm_current.motor1;
  ros_pid1_tel_msg.demanded_velocity=kinematic_rpm_demanded.motor1;
  ros_pid1_tel_msg.demanded_duty=0;
  ros_pid1_tel_pub.publish(&ros_pid1_tel_msg);

  ros_pid2_tel_msg.target_velocity=kinematic_rpm_target.motor2;
  ros_pid2_tel_msg.current_velocity=kinematic_rpm_current.motor2;
  ros_pid2_tel_msg.demanded_velocity=kinematic_rpm_demanded.motor2;
  ros_pid2_tel_msg.demanded_duty=0;
  ros_pid2_tel_pub.publish(&ros_pid2_tel_msg);

  ros_pid3_tel_msg.target_velocity=kinematic_rpm_target.motor3;
  ros_pid3_tel_msg.current_velocity=kinematic_rpm_current.motor3;
  ros_pid3_tel_msg.demanded_velocity=kinematic_rpm_demanded.motor3;
  ros_pid3_tel_msg.demanded_duty=0;
  ros_pid3_tel_pub.publish(&ros_pid3_tel_msg);

  ros_pid4_tel_msg.target_velocity=kinematic_rpm_target.motor4;
  ros_pid4_tel_msg.current_velocity=kinematic_rpm_current.motor4;
  ros_pid4_tel_msg.demanded_velocity=kinematic_rpm_demanded.motor4;
  ros_pid4_tel_msg.demanded_duty=0;
  ros_pid4_tel_pub.publish(&ros_pid4_tel_msg);
}

void ros_cmd_callback(const geometry_msgs::Twist & cmd_msg) {
  kinematic_velocities_t.linear_x = cmd_msg.linear.x;
  kinematic_velocities_t.linear_y = cmd_msg.linear.y;
  kinematic_velocities_t.angular_z = cmd_msg.angular.z;

  ros_prev_command_time = millis();
}

void ros_pid_callback(const robotin_project::PID& pid) {
  motor1_pid.updateConstants(pid.p, pid.i, pid.d);
  motor2_pid.updateConstants(pid.p, pid.i, pid.d);
  motor3_pid.updateConstants(pid.p, pid.i, pid.d);
  motor4_pid.updateConstants(pid.p, pid.i, pid.d);
}

//-------------------Robot base.
kinematic_velocities robot_base_move() {
  /*
  char buffer[50];
  sprintf (buffer, "%d %d %d %d",current_kinematic_rpm1,current_kinematic_rpm2,current_kinematic_rpm3,current_kinematic_rpm4);
  ros_nh.loginfo(buffer);
  */

  kinematic_rpm_target =  kinematic_direct(kinematic_velocities_t);

  kinematic_rpm_current.motor1 = motor1_encoder.getRPM();
  kinematic_rpm_current.motor2 = motor2_encoder.getRPM();
  kinematic_rpm_current.motor3 = motor3_encoder.getRPM();
  kinematic_rpm_current.motor4 = motor4_encoder.getRPM();

  kinematic_rpm_demanded.motor1 = motor1_pid.compute(kinematic_rpm_target.motor1, kinematic_rpm_current.motor1);
  kinematic_rpm_demanded.motor2 = motor2_pid.compute(kinematic_rpm_target.motor2, kinematic_rpm_current.motor2);
  kinematic_rpm_demanded.motor3 = motor3_pid.compute(kinematic_rpm_target.motor3, kinematic_rpm_current.motor3);
  kinematic_rpm_demanded.motor4 = motor4_pid.compute(kinematic_rpm_target.motor4, kinematic_rpm_current.motor4);

  kinematic_velocities kinematic_velocities_t = kinematic_inverse(kinematic_rpm_current);

  motor_drive_move(kinematic_rpm_demanded.motor1,MOTOR1_IN_A,MOTOR1_IN_B,MOTOR1_PWM);
  motor_drive_move(kinematic_rpm_demanded.motor2,MOTOR2_IN_A,MOTOR2_IN_B,MOTOR2_PWM);
  motor_drive_move(kinematic_rpm_demanded.motor3,MOTOR3_IN_A,MOTOR3_IN_B,MOTOR3_PWM);
  motor_drive_move(kinematic_rpm_demanded.motor4,MOTOR4_IN_A,MOTOR4_IN_B,MOTOR4_PWM);

  return kinematic_velocities_t;
}

void robot_base_stop() {
  kinematic_velocities_t.linear_x=0;
  kinematic_velocities_t.linear_y=0;
  kinematic_velocities_t.angular_z=0;
}

//-------------------Motor Drive.
void motor_drive_init(int motor_pinA_,int motor_pinB_,int pwm_pin_) {
  pinMode(pwm_pin_, OUTPUT);
  pinMode(motor_pinA_, OUTPUT);
  pinMode(motor_pinB_, OUTPUT);
}

void motor_drive_move(int pwm,int motor_pinA_,int motor_pinB_,int pwm_pin_) {
  if(pwm > 0) {
      digitalWrite(motor_pinA_, HIGH);
      digitalWrite(motor_pinB_, LOW);
  } else if(pwm < 0) {
      digitalWrite(motor_pinA_, LOW);
      digitalWrite(motor_pinB_, HIGH);
  }
  analogWrite(pwm_pin_, abs(pwm));
}

//-------------------Kinematic
kinematic_rpm kinematic_direct(const kinematic_velocities & kinematic_velocities_t)  {
  kinematic_rpm kinematic_rpm_t;

  float linear_vel_x_mins;
  float angular_vel_z_mins;
  float tangential_vel;
  float x_kinematic_rpm;
  float tan_kinematic_rpm;

  //convert m/s to m/min
  linear_vel_x_mins = kinematic_velocities_t.linear_x * 60;

  //convert rad/s to rad/min
  angular_vel_z_mins = kinematic_velocities_t.angular_z * 60;

  tangential_vel = angular_vel_z_mins * ((wheels_x_distance_ / 2) + (wheels_y_distance_ / 2));

  x_kinematic_rpm = linear_vel_x_mins / wheel_circumference_;
  tan_kinematic_rpm = tangential_vel / wheel_circumference_;
  
  //calculate for the target motor kinematic_rpm and direction
  //front-left motor
  kinematic_rpm_t.motor1 = x_kinematic_rpm  - tan_kinematic_rpm;
  kinematic_rpm_t.motor1 = constrain(kinematic_rpm_t.motor1, -max_kinematic_rpm_, max_kinematic_rpm_);

  //front-right motor
  kinematic_rpm_t.motor2 = x_kinematic_rpm  + tan_kinematic_rpm;
  kinematic_rpm_t.motor2 = constrain(kinematic_rpm_t.motor2, -max_kinematic_rpm_, max_kinematic_rpm_);

  //rear-left motor
  kinematic_rpm_t.motor3 = x_kinematic_rpm  - tan_kinematic_rpm;
  kinematic_rpm_t.motor3 = constrain(kinematic_rpm_t.motor3, -max_kinematic_rpm_, max_kinematic_rpm_);

  //rear-right motor
  kinematic_rpm_t.motor4 = x_kinematic_rpm  + tan_kinematic_rpm;
  kinematic_rpm_t.motor4 = constrain(kinematic_rpm_t.motor4, -max_kinematic_rpm_, max_kinematic_rpm_);

  return kinematic_rpm_t;
}

kinematic_velocities kinematic_inverse(const kinematic_rpm &kinematic_rpm_t) {
  kinematic_velocities kinematic_velocities_t;

  float average_rps_x;
  float average_rps_a;

  //convert average revolutions per minute to revolutions per second
  average_rps_x = ((float)(kinematic_rpm_t.motor1 + 
                            kinematic_rpm_t.motor2 + 
                            kinematic_rpm_t.motor3 + 
                            kinematic_rpm_t.motor4) / total_wheels_) / 60; // kinematic_rpm
  kinematic_velocities_t.linear_x = average_rps_x * wheel_circumference_; // m/s

  //convert average revolutions per minute in y axis to revolutions per second
  kinematic_velocities_t.linear_y = 0;

  //convert average revolutions per minute to revolutions per second
  average_rps_a = ((float)(-kinematic_rpm_t.motor1 + 
                            kinematic_rpm_t.motor2 - 
                            kinematic_rpm_t.motor3 + 
                            kinematic_rpm_t.motor4) / total_wheels_) / 60;
  kinematic_velocities_t.angular_z =  (average_rps_a * wheel_circumference_) / 
                                        ((wheels_x_distance_ / 2) + (wheels_y_distance_ / 2)); //  rad/s    

  return kinematic_velocities_t;
}