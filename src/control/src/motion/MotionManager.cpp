/*--------------------------------------------------------------------

******************************************************************************
* @file MotionManager.cpp
* @author ROBOTIS
* @version V2.0.0 - ROBOFEI-HT
* @created -
* @Modified Isaac Jesus da Silva - University Center FEI - 27/09/2016  😛
* @e-mail isaac25silva@yahoo.com.br
* @brief MotionManager 😛
****************************************************************************
**************************************************************************** 
---------------------------------------------------------------------------*/
// ros2 run control gait_publisher


#include <stdio.h>
#include <math.h>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include "FSR.h"
#include "MX28.h"
#include "MotionManager.h"
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <fstream>
#include <time.h>
#include <cstdlib>
#include "minIni.h"

#include "rclcpp/rclcpp.hpp"
#include "sensor_msgs/msg/imu.hpp"
#include "std_msgs/msg/bool.hpp"
#include "custom_interfaces/msg/set_position.hpp"
#include "custom_interfaces/msg/set_position_original.hpp"
#include "custom_interfaces/srv/get_position.hpp"
#include "custom_interfaces/msg/walk.hpp"
#include "custom_interfaces/msg/neck_position.hpp"

#define INI_FILE_PATH       "src/control/Data/config_reforced_prometheus.ini"

using namespace Robot;
using namespace std::chrono_literals;
using std::placeholders::_1;

// Torque adaption every second
const int TORQUE_ADAPTION_CYCLES = 1000 / MotionModule::TIME_UNIT;
const int DEST_TORQUE = 1023;
int position[20];

#define BROADCAST_ID        0xFE    // 254

//#define LOG_VOLTAGES 1
rclcpp::NodeOptions options;

float X_AMPLITUDE = 0;
float Y_AMPLITUDE = 0;
float A_AMPLITUDE = 0;
int last_movement = 0;
int walk=0;
int neck_up = 2047;
int neck_sides = 2047;

MotionManager* MotionManager::m_UniqueInstance = new MotionManager(options);

MotionManager::MotionManager(const rclcpp::NodeOptions & options) :
        m_ProcessEnable(false),
        m_IsRunning(false),
        m_IsThreadRunning(false),
        m_IsLogging(false),
				m_torqueAdaptionCounter(TORQUE_ADAPTION_CYCLES),
				m_voltageAdaptionFactor(1.0),
        DEBUG_PRINT(false),
		rclcpp::Node("gait_publisher", options)
{
	
	subscription_imu = this->create_subscription<sensor_msgs::msg::Imu>("imu/data", 10, std::bind(&MotionManager::topic_callback, this, _1));
	subscription_walk = this->create_subscription<custom_interfaces::msg::Walk>("walking", 10, std::bind(&MotionManager::topic_callback_walk, this, _1));
	subscription_positions = this->create_subscription<custom_interfaces::msg::SetPosition>("set_position", 10, std::bind(&MotionManager::topic_callback_positions, this, _1));
	subscription_neck = this->create_subscription<custom_interfaces::msg::NeckPosition>("/neck_position", 10, std::bind(&MotionManager::topic_callback_neck, this, _1));
	publisher_ = this->create_publisher<custom_interfaces::msg::SetPosition>("set_position", 10); 
	publisher_fase_zero = this->create_publisher<std_msgs::msg::Bool>("/fase_zero", 10); 
	publisher_single = this->create_publisher<custom_interfaces::msg::SetPositionOriginal>("set_position_single", 10); 
	client = this->create_client<custom_interfaces::srv::GetPosition>("get_position");
    timer_ = this->create_wall_timer(4ms, std::bind(&MotionManager::Process, this));
	keep_walking = false;
	for(int i = 0; i < JointData::NUMBER_OF_JOINTS; i++)
        m_Offset[i] = 0;
	// update_thread_ = std::thread(std::bind(&MotionManager::update_loop, this));
	printf("CONSTRUTOR\n");
	ini = new minIni((char *)INI_FILE_PATH);
	
}

void MotionManager::topic_callback_neck(const std::shared_ptr<custom_interfaces::msg::NeckPosition> neck_msg) const
    {
      neck_sides = neck_msg->position19;
      neck_up = neck_msg->position20;
    }

void MotionManager::update_loop(void)
{
  while (rclcpp::ok())
  {
   RCLCPP_INFO(this->get_logger(), "Running motion manager");
   this->Process();
  }  
}

MotionManager::~MotionManager()
{
}

void MotionManager::topic_callback(const std::shared_ptr<sensor_msgs::msg::Imu> imu_msg_) const
    {
        float IMU_GYRO_X = -imu_msg_->angular_velocity.x/10;
        float IMU_GYRO_Y  = -imu_msg_->angular_velocity.y/10;
	}

void MotionManager::topic_callback_positions(const std::shared_ptr<custom_interfaces::msg::SetPosition> position_msg_) const
    {
		for (int i=0; i<20; i++)
        	position[i] = position_msg_->position[i];
	}

void MotionManager::GetIniParameter()
{
	if (Walking::GetInstance()->GetCurrentPhase()==0 || Walking::GetInstance()->GetCurrentPhase()==2){
		if (walk != last_movement){
			if (walk == 1){
				printf("CALLBACK WALK\n");
				Walking::GetInstance()->LoadINISettings(ini, "Walking Config");
			}
			else if (walk == 2){
				printf("CALLBACK GAIT\n");
				Walking::GetInstance()->LoadINISettings(ini, "Gait");
			}
			else if (walk == 3){
				printf("CALLBACK TURN\n");
				Walking::GetInstance()->LoadINISettings(ini, "Turn Robot");
			}
			else if (walk == 4){
				printf("CALLBACK WALK SLOW\n");
				Walking::GetInstance()->LoadINISettings(ini, "Walk Slow");
			}
			else if (walk == 5){
				printf("CALLBACK TURN BALL RIGHT\n");
				Walking::GetInstance()->LoadINISettings(ini, "Turn Ball Right");
			}
			else if (walk == 6){
				printf("CALLBACK TURN BALL LEFT\n");
				Walking::GetInstance()->LoadINISettings(ini, "Turn Ball Left");
			}
			else if (walk == 7){
				printf("CALLBACK SIDLE RIGHT\n");
				Walking::GetInstance()->LoadINISettings(ini, "Sidle Right");
			}
			else if (walk == 8){
				printf("CALLBACK SIDLE LEFT\n");
				Walking::GetInstance()->LoadINISettings(ini, "Sidle Left");
			}
			else if (walk == 9){
				printf("CALLBACK WALK BACKWARD\n");
				Walking::GetInstance()->LoadINISettings(ini, "Walking Backward");
			}
			else if (walk == 10){
				printf("CALLBACK WALK BACKWARD SLOW\n");
				Walking::GetInstance()->LoadINISettings(ini, "Walking Backward Slow");
			}
			else if (walk == 11){
				printf("CALLBACK TURN ROBOT RIGHT\n");
				Walking::GetInstance()->LoadINISettings(ini, "Turn Robot Right");
			}
			else if (walk == 12){
				printf("CALLBACK TURN ROBOT LEFT\n");
				Walking::GetInstance()->LoadINISettings(ini, "Turn Robot Left");
			}
			else if (walk == 13){
				printf("CALLBACK TURN ROBOT LEFT\n");
				Walking::GetInstance()->LoadINISettings(ini, "Turn Robot Left slow");
			}
			last_movement = walk;
			MotionManager::GetInstance()->LoadINISettings(ini);
			Action::GetInstance()->Stop();

			//MotionManager::GetInstance()->AddModule((MotionModule*)Walking::GetInstance());
			MotionManager::GetInstance()->Initialize();
			Walking::GetInstance()->m_Joint.SetEnableBody(true);
			Action::GetInstance()->m_Joint.SetEnableBody(false);
			MotionStatus::m_CurrentJoints.SetEnableBodyWithoutHead(true);
			MotionManager::GetInstance()->SetEnable(true);
			printf("%d\n", MotionManager::GetInstance()->GetEnable());
			Walking::GetInstance()->Start();
			// printf("WALKING %d\n", Walking::GetInstance()->IsRunning());
			// printf("ACTION %d\n", Action::GetInstance()->IsRunning());
			MotionManager::GetInstance()->keep_walking=true;
			//printf("KEEP WALKING DEPOIS %d\n", MotionManager::GetInstance()->keep_walking);
		}
	}
}

void MotionManager::topic_callback_walk(const std::shared_ptr<custom_interfaces::msg::Walk> walk_msg_) const
    {
		RCLCPP_INFO(this->get_logger(), "CALLBACK WALK MM");
		auto message_fase = std_msgs::msg::Bool();
		message_fase.data = false;
        walk = walk_msg_->walk_number;
		if (walk!=0){
			message_fase.data = true;
			MotionManager::GetInstance()->GetIniParameter();
		}
		else{ // parar o walking
			RCLCPP_INFO(this->get_logger(), "NO MOTION MANAGER");
			if (MotionManager::GetInstance()->keep_walking==false && Walking::GetInstance()->GetCurrentPhase()==0){
				message_fase.data = true;
				Action::GetInstance()->Stop();
				MotionManager::GetInstance()->AddModule((MotionModule*)Walking::GetInstance());
				// MotionManager::GetInstance()->Initialize();
				Walking::GetInstance()->m_Joint.SetEnableBody(false);
				Action::GetInstance()->m_Joint.SetEnableBody(false);
				MotionStatus::m_CurrentJoints.SetEnableBodyWithoutHead(true);
				MotionManager::GetInstance()->SetEnable(true);
				printf("%d\n", MotionManager::GetInstance()->GetEnable());
				Walking::GetInstance()->Stop();
				// printf("WALKING %d\n", Walking::GetInstance()->IsRunning());
				// printf("ACTION %d\n", Action::GetInstance()->IsRunning());
				MotionManager::GetInstance()->keep_walking=true;
			}
		}
		publisher_fase_zero->publish(message_fase);
	}

bool MotionManager::Initialize(bool fadeIn)
{
	//rclcpp::spin_some(std::make_shared<GaitPublisher>());
	int value, error;
	printf("INITIALIZE\n");

	usleep(10000);
	m_Enabled = false;
	m_ProcessEnable = true;

	
	for(int id=JointData::ID_MIN; id<=JointData::ID_MAX-2; id++) //diminui tirando a cabeça
	{
		if(DEBUG_PRINT == true)
			fprintf(stderr, "ID:%d initializing...", id);
		MotionStatus::m_CurrentJoints.SetValue(id, position[id]);
		std::cout << position[id] << std::endl;
	}

	if(fadeIn)
	{
		//for(int i=JointData::ID_R_SHOULDER_PITCH; i<JointData::NUMBER_OF_JOINTS; i++)
			//cm730->WriteWord(i, MX28::P_TORQUE_LIMIT_L, 0, 0);
	}

	m_fadeIn = fadeIn;
	m_torque_count = 0;

	m_CalibrationStatus = 0;
	m_FBGyroCenter = 512;
	m_RLGyroCenter = 512;

	return true;
}

bool MotionManager::Reinitialize()
{
	m_ProcessEnable = false;

	//m_CM730->DXLPowerOn();

	int value, error;
	for(int id=JointData::ID_MIN; id<=JointData::ID_MAX-2; id++)//tirando a cabeça
	{
		if(DEBUG_PRINT == true)
			fprintf(stderr, "ID:%d initializing...", id);
		
		// if(m_CM730->ReadWord(id, MX28::P_PRESENT_POSITION_L, &value, &error) == CM730::SUCCESS)
		// {
		// 	MotionStatus::m_CurrentJoints.SetValue(id, value);
		// 	MotionStatus::m_CurrentJoints.SetEnable(id, true);

		// 	if(DEBUG_PRINT == true)
		// 		fprintf(stderr, "[%d] Success\n", value);
		// }
		else
		{
			MotionStatus::m_CurrentJoints.SetEnable(id, false);

			if(DEBUG_PRINT == true)
				fprintf(stderr, " Fail\n");
		}
	}

	m_ProcessEnable = true;
	return true;
}

void MotionManager::Restartrobot()
{
	m_torque_count=0;
}

void MotionManager::StartLogging()
{
    char szFile[32] = {0,};

    int count = 0;
    while(1)
    {
        sprintf(szFile, "Logs/Log%d.csv", count);
        if(0 != access(szFile, F_OK))
            break;
        count++;
		if(count > 256) return;
    }
		m_LogFileStream.open(szFile, std::ios::out);
    for(int id = JointData::ID_MIN; id <= JointData::ID_MAX-2; id++)//tirando a cabeça
        m_LogFileStream << "nID_" << id << "_GP,nID_" << id << "_PP,";
    m_LogFileStream << "GyroFB,GyroRL,AccelFB,AccelRL,L_FSR_X,L_FSR_Y,R_FSR_X,R_FSR_Y," << "\x0d\x0a";

    m_IsLogging = true;
}

void MotionManager::StopLogging()
{
    m_IsLogging = false;
    m_LogFileStream.close();
}

void MotionManager::LoadINISettings(minIni* ini)
{
    LoadINISettings(ini, OFFSET_SECTION);
}
void MotionManager::LoadINISettings(minIni* ini, const std::string &section)
{
    int ivalue = INVALID_VALUE;

    for(int i = JointData::ID_MIN; i <= JointData::ID_MAX-2; i++)//tirando a cabeça
    {
        char key[10];
        sprintf(key, "ID_%.2d", i);
        if((ivalue = ini->geti(section, key, INVALID_VALUE)) != INVALID_VALUE)  MotionManager::GetInstance()->m_Offset[i] = ivalue;
		printf("OFFSET %d\n", MotionManager::GetInstance()->m_Offset[i]);
    }
		m_angleEstimator.LoadINISettings(ini, section + "_angle");
}
void MotionManager::SaveINISettings(minIni* ini)
{
    SaveINISettings(ini, OFFSET_SECTION);
}
void MotionManager::SaveINISettings(minIni* ini, const std::string &section)
{
    for(int i = JointData::ID_MIN; i <= JointData::ID_MAX-2; i++)//tirando a cabeça
    {
        char key[10];
        sprintf(key, "ID_%.2d", i);
        ini->put(section, key, m_Offset[i]);
    }
		m_angleEstimator.SaveINISettings(ini, section + "_angle");
}

#define GYRO_WINDOW_SIZE    100
#define ACCEL_WINDOW_SIZE   30
#define MARGIN_OF_SD        2.0
void MotionManager::Process()
{
	// RCLCPP_INFO(this->get_logger(), "FASE PROCESS %d", Walking::GetInstance()->GetCurrentPhase());
	if (Walking::GetInstance()->GetCurrentPhase()==0 || Walking::GetInstance()->GetCurrentPhase()==2){
		auto message_fase = std_msgs::msg::Bool();
		message_fase.data = true;
		publisher_fase_zero->publish(message_fase);
	}
	
	if(walk!=0 || (walk==0 && (Walking::GetInstance()->GetCurrentPhase()!=0 || Walking::GetInstance()->GetCurrentPhase()==2))){
		if((Walking::GetInstance())->m_Joint.GetEnable(5) == true)
		{
			Walking::GetInstance()->Process();
		}
		
		if (walk != last_movement)
			MotionManager::GetInstance()->GetIniParameter();

		if(m_fadeIn && m_torque_count < DEST_TORQUE) {
			if(m_torque_count < 100)
				m_torque_count += 3;
			else
				m_torque_count = 2047 ;

			//m_CM730->WriteWord(CM730::ID_BROADCAST, MX28::P_TORQUE_LIMIT_L, m_torque_count, 0);

			auto message_single = custom_interfaces::msg::SetPositionOriginal(); 
			message_single.id = BROADCAST_ID;
			message_single.address = MX28::P_GOAL_CURRENT;
			message_single.position = m_torque_count;
			publisher_single->publish(message_single);
			// m_CM730->write2ByteTxRx(portHandler, BROADCAST_ID, MX28::P_GOAL_CURRENT, m_torque_count, &dxl_error);
			
			if(m_torque_count == 2047)
			{
				RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "torque 2047");
				message_single.id = 3;
				message_single.address = MX28::P_GOAL_CURRENT;
				message_single.position = 1941;
				publisher_single->publish(message_single);
				//m_CM730->write2ByteTxRx(portHandler, 3, MX28::P_GOAL_CURRENT, 1941, &dxl_error);
				message_single.id = 4;
				message_single.address = MX28::P_GOAL_CURRENT;
				message_single.position = 1941;
				publisher_single->publish(message_single);
				//m_CM730->write2ByteTxRx(portHandler, 4, MX28::P_GOAL_CURRENT, 1941, &dxl_error);
				message_single.id = 5;
				message_single.address = MX28::P_GOAL_CURRENT;
				message_single.position = 1941;
				publisher_single->publish(message_single);
				//m_CM730->write2ByteTxRx(portHandler, 5, MX28::P_GOAL_CURRENT, 1941, &dxl_error);
				message_single.id = 6;
				message_single.address = MX28::P_GOAL_CURRENT;
				message_single.position = 1941;
				publisher_single->publish(message_single);
				//m_CM730->write2ByteTxRx(portHandler, 6, MX28::P_GOAL_CURRENT, 1941, &dxl_error);
			}
		}
		// if(m_fadeIn && m_torque_count < DEST_TORQUE) {
		//     m_CM730->WriteWord(CM730::ID_BROADCAST, MX28::P_TORQUE_LIMIT_L, m_torque_count, 0);
		//     m_torque_count += 2;
		// }
		if(MotionManager::GetInstance()->m_ProcessEnable == false || MotionManager::GetInstance()->m_IsRunning == true)
			return;
			
		MotionManager::GetInstance()->m_IsRunning = true;

		MotionManager::GetInstance()->m_CalibrationStatus = 1;

		// if(m_CalibrationStatus == 1 && m_Enabled == true)
		if(MotionManager::GetInstance()->m_CalibrationStatus == 1 && MotionManager::GetInstance()->GetEnable() == true)
		{

			const double GYRO_ALPHA = 0.1;
			int gyroValFB = (int) (IMU_GYRO_Y);
			int gyroValRL = (int) (IMU_GYRO_X);

			MotionStatus::FB_GYRO = (1.0 - GYRO_ALPHA) * MotionStatus::FB_GYRO + GYRO_ALPHA * gyroValFB;
			MotionStatus::RL_GYRO = (1.0 - GYRO_ALPHA) * MotionStatus::RL_GYRO + GYRO_ALPHA * gyroValRL;

			for(int id=JointData::ID_MIN; id<=JointData::ID_MAX-2; id++)
			{
				if((Walking::GetInstance())->m_Joint.GetEnable(id) == true)
				{
					MotionStatus::m_CurrentJoints.SetValue(id, (Walking::GetInstance())->m_Joint.GetValue(id));
					//printf("ACTION  %d: %d", id, (*i)->m_Joint.GetValue(id));
					MotionStatus::m_CurrentJoints.SetPGain(id, (Walking::GetInstance())->m_Joint.GetPGain(id));
					MotionStatus::m_CurrentJoints.SetIGain(id, (Walking::GetInstance())->m_Joint.GetIGain(id));
					MotionStatus::m_CurrentJoints.SetDGain(id, (Walking::GetInstance())->m_Joint.GetDGain(id));
				// MotionStatus::m_CurrentJoints.SetSlope(id, (*i)->m_Joint.GetCWSlope(id), (*i)->m_Joint.GetCCWSlope(id));
				// MotionStatus::m_CurrentJoints.SetValue(id, (*i)->m_Joint.GetValue(id));
				}
			}
			auto message = custom_interfaces::msg::SetPosition();  
			int param[JointData::NUMBER_OF_JOINTS * MX28::PARAM_BYTES];
			int joint_num = 0;
			int pos[18];
			for(int id=JointData::ID_MIN; id<=JointData::ID_MAX-2; id++) // loop que vai de 1 até 18
					{
					param[id] = id;
					pos[id] = MotionStatus::m_CurrentJoints.GetValue(id)+ MotionManager::GetInstance()->m_Offset[id];
					
					
					if(DEBUG_PRINT == true)
					fprintf(stderr, "ID[%d] : %d \n", id, MotionStatus::m_CurrentJoints.GetValue(id));
					}
			message.id = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};          
			message.position = {pos[1], pos[2], pos[3], pos[4], pos[5], pos[6], pos[7], pos[8], pos[9], pos[10], pos[11], pos[12], pos[13], pos[14], pos[15], pos[16], pos[17], pos[18], neck_sides, neck_up};   
			publisher_->publish(message);

		}
		else
			RCLCPP_INFO(rclcpp::get_logger("rclcpp"), "nao entrou no segundo if");

		MotionManager::GetInstance()->m_IsRunning = false;
		// if(m_torque_count != DEST_TORQUE && --m_torqueAdaptionCounter == 0)
		// {
		//     m_torqueAdaptionCounter = TORQUE_ADAPTION_CYCLES;
		//     adaptTorqueToVoltage();
		// }

		//rclcpp::spin_some(std::make_shared<GaitPublisher>());
	}
}

void MotionManager::SetEnable(bool enable)
{
	printf("Set enable \n");
	m_Enabled = enable;
}

void MotionManager::AddModule(MotionModule *module)
{
	printf("ADD MODULE \n");
	module->Initialize();
	m_Modules.push_back(module);

}

void MotionManager::RemoveModule(MotionModule *module)
{
	m_Modules.remove(module);
}

void MotionManager::SetJointDisable(int index)
{
    if(m_Modules.size() != 0)
    {
        for(std::list<MotionModule*>::iterator i = m_Modules.begin(); i != m_Modules.end(); i++)
            (*i)->m_Joint.SetEnable(index, false);
    }
}

#include "rclcpp_components/register_node_macro.hpp"
RCLCPP_COMPONENTS_REGISTER_NODE(Robot::MotionManager)


// void MotionManager::adaptTorqueToVoltage()
// {
// 	static int count_fail=0;
// 	static int count_volt=0;
//     const int DEST_TORQUE = 1023;
//     const int FULL_TORQUE_VOLTAGE = 210; // 13V - at 13V darwin will make no adaptation as the standard 3 cell battery is always below this voltage, this implies Nimbro-OP runs on 4 cells

//     int voltage;
// 		// torque is only reduced if it is greater then FULL_TORQUE_VOLTAGE
// 	if(m_CM730->ReadByte(7, 42, &voltage, 0) != CM730::SUCCESS && m_CM730->ReadByte(8, 42, &voltage, 0) != CM730::SUCCESS)
// 	{
//     	count_fail++;
//     	if(count_fail>=4)
//     	{
//     		printf("Falha na comunicação: Chave provavelmente desligada\n");
//     		logServo(); //Escreve no arquivo de log a hora que terminou o processo
//     		exit(0);
//     	}
//        	return;
//     }
//     count_fail=0;

//     if(voltage < 160)
//     {
//         count_volt++;
//         if(count_volt>=4)
//         {
// 		    printf("Tensão Abaixo do recomendado | Tensão = %2.1fV\n", (float)voltage/(float)10);
// 		    printf("A bateria deve ser trocada\n");
// 		    logVoltage(voltage); //Escreve no arquivo de log a tensão e a hora que terminou o processo
// 		    exit(0);
// 		}
//     }
//     else
//         count_volt=0;
//     write_int(mem, VOLTAGE, voltage);

//     if(m_CM730->ReadByte(200, CM730::P_VOLTAGE, &voltage, 0) != CM730::SUCCESS)
//         return;

//     voltage = (voltage > FULL_TORQUE_VOLTAGE) ? voltage : FULL_TORQUE_VOLTAGE;
//     m_voltageAdaptionFactor = ((double)FULL_TORQUE_VOLTAGE) / voltage;
//     int torque = m_voltageAdaptionFactor * DEST_TORQUE;

// #if LOG_VOLTAGES
//     fprintf(m_voltageLog, "%3d       %4d\n", voltage, torque);
// #endif

//     m_CM730->WriteWord(CM730::ID_BROADCAST, MX28::P_TORQUE_LIMIT_L, torque, 0);
// }

// void MotionManager::logVoltage(int voltage)
// {
//         std::fstream File;
//         time_t _tm =time(NULL);
//         struct tm * curtime = localtime ( &_tm );
//         File.open("../../Control/Control.log", std::ios::app | std::ios::out);
//         if (File.good() && File.is_open())
//         {
//             File << "Tensão Abaixo do recomendado | Tensão = "<<(float)voltage/(float)10<<"V --- ";
//             File << asctime(curtime);
//             File.flush();
//             File.close();
//         }
//         else
// 	        printf("Erro ao Salvar o arquivo\n");
// }

// void MotionManager::logServo()
// {
//         std::fstream File;
//         time_t _tm =time(NULL);
//         struct tm * curtime = localtime ( &_tm );
//         File.open("../../Control/Control.log", std::ios::app | std::ios::out);
//         if (File.good() && File.is_open())
//         {
//             File << "Falha na comunicação: Chave provavelmente desligada"<<" --- ";
//             File << asctime(curtime);
//             File.flush();
//             File.close();
//         }
//         else
// 	        printf("Erro ao Salvar o arquivo\n");
// }