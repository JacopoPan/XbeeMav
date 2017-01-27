/* CommunicationManager.cpp -- Communication Manager class for XBee:
			       Handles all communications with other ROS nodes
			       and the serial port --     	                             */
/* ------------------------------------------------------------------------- */
/* September 20, 2016 -- @Copyright Aymen Soussia. All rights reserved.      */
/*                                  (aymen.soussia@gmail.com)                */


#include "CommunicationManager.h"


uint16_t* u64_cvt_u16(uint64_t u64){
   uint16_t* out = new uint16_t[4];
   uint32_t int32_1 = u64 & 0xFFFFFFFF;
   uint32_t int32_2 = (u64 & 0xFFFFFFFF00000000 ) >> 32;
    out[0] = int32_1 & 0xFFFF;
    out[1] = (int32_1 & (0xFFFF0000) ) >> 16;
    out[2] = int32_2 & 0xFFFF;
    out[3] = (int32_2 & (0xFFFF0000) ) >> 16;
   //cout << " values " <<out[0] <<"  "<<out[1] <<"  "<<out[2] <<"  "<<out[3] <<"  ";
return out;

}

namespace Mist
{


namespace Xbee
{


//*****************************************************************************
CommunicationManager::CommunicationManager():
	START_DLIMITER(static_cast<unsigned char>(0x7E)),
	LOOP_RATE(10) /* 10 fps */
{
}


//*****************************************************************************
CommunicationManager::~CommunicationManager()
{
}


//*****************************************************************************
bool CommunicationManager::Init(
		const std::string& device, const std::size_t baud_rate)
{
	if (serial_device_.Init(device, baud_rate))
	{
		in_messages_ = serial_device_.Get_In_Messages_Pointer();
		return true;
	}
	else
	{
		Display_Init_Communication_Failure();
		return false;
	}
}



//*****************************************************************************
void CommunicationManager::Run(DRONE_TYPE drone_type,
		RUNNING_MODE running_mode)
{
	std::thread thread_service(&SerialDevice::Run_Service, &serial_device_);

	Display_Drone_Type_and_Running_Mode(drone_type, running_mode);

	if (RUNNING_MODE::SWARM == running_mode)
		Run_In_Swarm_Mode();
	else
		Run_In_Solo_Mode(drone_type);

	serial_device_.Stop_Service();
  	thread_service.join();
}


//*****************************************************************************
void CommunicationManager::Run_In_Solo_Mode(DRONE_TYPE drone_type)
{
	std::string service_name;
	bool success = false;

	if (DRONE_TYPE::MASTER == drone_type)
	{
		if (node_handle_.getParam("Xbee_In_From_Controller", service_name))
		{
			mav_dji_server_ = node_handle_.advertiseService(service_name.c_str(), 		&CommunicationManager::Serve_Flight_Controller, this);
			success = true;
		}
		else
			std::cout << "Failed to Get Service Name: param 'Xbee_In_From_Controller' Not Found." << std::endl;
	}
	else
 	{
		if (node_handle_.getParam("Xbee_Out_To_Controller", service_name))
		{
			mav_dji_client_ = node_handle_.serviceClient<mavros_msgs::CommandInt>(service_name.c_str());
			success = true;
		}
		else
			std::cout << "Failed to Get Service Name: param 'Xbee_Out_To_Controller' Not Found." << std::endl;
	}

	if (success)
	{
		ros::Rate loop_rate(LOOP_RATE);

		while (ros::ok())
		{
			Check_In_Messages_and_Transfer_To_Server();
			ros::spinOnce();
	    		loop_rate.sleep();
		}
	}

}


//*****************************************************************************
void CommunicationManager::Run_In_Swarm_Mode()
{
	std::string out_messages_topic;
	std::string in_messages_topic;
	bool success_1 = false;
	bool success_2 = false;

	if (node_handle_.getParam("Xbee_In_From_Buzz", out_messages_topic))
	{
		mavlink_subscriber_ = node_handle_.subscribe(out_messages_topic.c_str(), 1000,
			&CommunicationManager::Send_Mavlink_Message_Callback, this);
		success_1 = true;
	}
	else
		std::cout << "Failed to Get Topic Name: param 'Xbee_In_From_Buzz' Not Found." << std::endl;

	if (node_handle_.getParam("Xbee_Out_To_Buzz", in_messages_topic))
	{
		mavlink_publisher_ = node_handle_.advertise<mavros_msgs::Mavlink>(
			in_messages_topic.c_str(), 1000);
		success_2 = true;
	}
	else
		std::cout << "Failed to Get Topic Name: param 'Xbee_Out_To_Buzz' Not Found." << std::endl;

	if (success_1 && success_2)
	{
	
		ros::Rate loop_rate(LOOP_RATE);

		while (ros::ok())
		{
			Check_In_Messages_and_Transfer_To_Topics();
			ros::spinOnce();
	    		loop_rate.sleep();
		}
	}	
}


//*****************************************************************************
inline bool CommunicationManager::Serve_Flight_Controller(mavros_msgs::CommandInt::
		Request& request, mavros_msgs::CommandInt::Response& response)
{
	const std::size_t MAX_BUFFER_SIZE = 255;
	unsigned int command = 0;
	char temporary_buffer[MAX_BUFFER_SIZE];
	std::string frame;

	switch(request.command)
	{
		case mavros_msgs::CommandCode::NAV_TAKEOFF:
		{
			command = static_cast<unsigned int>(mavros_msgs::CommandCode::NAV_TAKEOFF);
			sprintf(temporary_buffer, "%u ", command);
			response.success = true;
			break;
		}
		case mavros_msgs::CommandCode::NAV_LAND:
		{
			command = static_cast<unsigned int>(mavros_msgs::CommandCode::NAV_LAND);
			sprintf(temporary_buffer, "%u ", command);
			response.success = true;
			break;	
		}
		case mavros_msgs::CommandCode::NAV_RETURN_TO_LAUNCH:
		{
			command = static_cast<unsigned int>(mavros_msgs::CommandCode::NAV_RETURN_TO_LAUNCH);
			sprintf(temporary_buffer, "%u ", command);
			response.success = true;
			break;	
		}
		case mavros_msgs::CommandCode::NAV_WAYPOINT:
		{
			command = static_cast<unsigned int>(mavros_msgs::CommandCode::NAV_WAYPOINT);
			sprintf(temporary_buffer, "%u %u %u %lf %u %u ",
					command, request.x, request.y, request.z, 0, 1); // TO DO change 0 and 1
			response.success = true;
			break;	
		}
		case mavros_msgs::CommandCode::CMD_MISSION_START:
		{
			command = static_cast<unsigned int>(mavros_msgs::CommandCode::CMD_MISSION_START);
			sprintf(temporary_buffer, "%u ", command);
			response.success = true;
			break;
		}
		default:
			response.success = false;		
	}

	if (command != 0)
	{
		Generate_Transmit_Request_Frame(temporary_buffer, &frame);
		serial_device_.Send_Frame(frame);
	}
	
	return true;
}


//*****************************************************************************
void CommunicationManager::Display_Init_Communication_Failure()
{
	std::cout << "Failed to Init Communication With XBee." << std::endl;
	std::cout << "Please Check The Following Parameters:" << std::endl;
	std::cout << "   1) Device (e.g. /dev/ttyUSB0 for Linux. " << std::endl;
	std::cout << "   2) Baud Rate." << std::endl;
	std::cout << "   3) Press Reset Button on The XBee." << std::endl;
	std::cout << "   4) Connect and Disconnect The XBee." << std::endl;
	std::cout << "   5) Update The XBee Firmware." << std::endl;
	std::cout << "   6) Reinstall The FTDI Driver." << std::endl;
}


//*****************************************************************************
inline void CommunicationManager::Generate_Transmit_Request_Frame(
		const char* const message,
		std::string * frame,
		const unsigned char frame_ID,
		const std::string& destination_adssress,
		const std::string& short_destination_adress,
		const std::string& broadcast_radius,
		const std::string& options)
{
	const unsigned char FAME_TYPE = static_cast<unsigned char>(0x10); /* Transmit Request Frame */
	std::string frame_parameters;
	std::string bytes_frame_parameters;

	frame_parameters.append(destination_adssress);
	frame_parameters.append(short_destination_adress);
	frame_parameters.append(broadcast_radius);
	frame_parameters.append(options);

	Convert_HEX_To_Bytes(frame_parameters, &bytes_frame_parameters);

	frame->push_back(FAME_TYPE);
	frame->push_back(frame_ID);
	frame->append(bytes_frame_parameters);
	frame->append(message, std::strlen(message));

	Calculate_and_Append_Checksum(frame);
	Add_Length_and_Start_Delimiter(frame);
}


//*****************************************************************************
inline void CommunicationManager::Convert_HEX_To_Bytes(
		const std::string& HEX_data, std::string* converted_data)
{
	const unsigned short TWO_BYTES = 2;
	char temporary_buffer[TWO_BYTES];
	unsigned char temporary_char;
	int temporary_HEX;

	for (unsigned short i = 0; i <= HEX_data.size() - TWO_BYTES;
			i += TWO_BYTES)
	{
		memcpy(temporary_buffer, HEX_data.c_str() + i, TWO_BYTES);
		sscanf(temporary_buffer, "%02X", &temporary_HEX);
		temporary_char = static_cast<unsigned char>(temporary_HEX);
		converted_data->push_back(temporary_char);
	}
}


//*****************************************************************************
inline void CommunicationManager::Calculate_and_Append_Checksum(
		std::string* frame)
{
	unsigned short bytes_sum = 0;
	unsigned lowest_8_bits = 0;
	unsigned short checksum = 0;
	unsigned char checksum_byte;

	for (unsigned short i = 0; i < frame->size(); i++)
		bytes_sum += static_cast<unsigned short>(frame->at(i));

	lowest_8_bits = bytes_sum & 0xFF;
	checksum = 0xFF - lowest_8_bits;
	checksum_byte = static_cast<unsigned char>(checksum);
	frame->push_back(checksum_byte);
}

unsigned short CommunicationManager::Caculate_Checksum(std::string* frame)
{

	unsigned short bytes_sum = 0;
	unsigned lowest_8_bits = 0;
	unsigned short checksum = 0;
	//unsigned char checksum_byte;

	for (unsigned short i = 0; i < frame->size(); i++)
		bytes_sum += static_cast<unsigned short>(frame->at(i));

	lowest_8_bits = bytes_sum & 0xFF;
	checksum = 0xFF - lowest_8_bits;
	
return checksum;
}

//*****************************************************************************
inline void CommunicationManager::Add_Length_and_Start_Delimiter(
		std::string* frame)
{
	const unsigned short FIVE_BYTES = 5;
	char temporary_buffer[FIVE_BYTES];
	unsigned char temporary_char;
	int Hex_frame_length_1;
	int Hex_frame_length_2;
	std::string header;
	int frame_length = frame->size() - 1; /* frame_length = frame_size - checksum byte */

	header.push_back(START_DLIMITER);
	sprintf(temporary_buffer, "%04X", frame_length);
	sscanf(temporary_buffer, "%02X%02X", &Hex_frame_length_1, 
			&Hex_frame_length_2);
	temporary_char = static_cast<unsigned char>(Hex_frame_length_1);
	header.push_back(temporary_char);
	temporary_char = static_cast<unsigned char>(Hex_frame_length_2);
	header.push_back(temporary_char);
	frame->insert(0, header);
}


//*****************************************************************************
inline void CommunicationManager::Check_In_Messages_and_Transfer_To_Topics()
{
	std::size_t size_in_messages = in_messages_->Get_Size();
	uint16_t* header;
	if (size_in_messages > 0)
	{
		//steps++;
		//if(steps==100) steps=0;
		uint64_t current_int64 = 0;
		for (std::size_t j = 0; j < size_in_messages; j++)
		{
			std::shared_ptr<std::string> in_message =
					in_messages_->Pop_Front();
			mavros_msgs::Mavlink mavlink_msg;
			
			//uint64_t header=0;
			sscanf(in_message->c_str(), "%" PRIu64 " ",
							&current_int64);
			header = u64_cvt_u16(current_int64);
			std::cout << "Received header" <<header[0]<<"  "<<header[1]<<"  "<<header[2]<<"  "<<header[3]<<"  "<< std::endl;
			if(header[3]==1){
				for (std::size_t i = 1; i < in_message->size(); i++)
				{
				
					if (' ' == in_message->at(i) || 0 == i)
					{
						sscanf(in_message->c_str() + i, "%" PRIu64 " ",
								&current_int64);
						mavlink_msg.payload64.push_back(current_int64);
					}
		
				}
				std::cout << "Single packet message received" << std::endl;
				mavlink_publisher_.publish(mavlink_msg);
				delete[] header;
			}
			else{
				std::cout << "Multi packet message" << std::endl;
				if (multi_msgs.empty() && header[0] == 0){
				std::cout << "first message" << std::endl;
				multi_msgs.insert(make_pair(header[2], in_message));
				cur_checksum=header[1];
				counter=1;
				}
				else if (header[1]==cur_checksum && header[0] == 0) {
					std::map< int, std::shared_ptr<std::string> >::iterator it = multi_msgs.find(header[2]);
					if(it!=multi_msgs.end()){
						multi_msgs.erase(it);
						multi_msgs.insert(make_pair(header[2], in_message));
						}
					else{
						multi_msgs.insert(make_pair(header[2], in_message));
						counter++;
					}
					std::cout << "multi msg counter" <<counter << std::endl;					
					/*If the total size of msg reached transfer to topic*/
					if(counter==header[3]){
						
						for(int i =1; i<=header[3];i++){
							it = multi_msgs.find(i);
								for (int j = 1; j < it->second->size(); j++)
								{
				
									if (' ' == it->second->at(j) || 0 == j)
									{
										sscanf(it->second->c_str() + j, "%" PRIu64 " ",
												&current_int64);
										mavlink_msg.payload64.push_back(current_int64);
									}
		
								}
						}
						
					std::cout << "one multi message published in topic" << std::endl;
					mavlink_publisher_.publish(mavlink_msg);
					multi_msgs.clear();
					cur_checksum=0;
					}
					steps=0;
					
				}
				delete[] header;
			}
		}
		
	}
}


//*****************************************************************************
inline void CommunicationManager::Check_In_Messages_and_Transfer_To_Server()
{
	std::size_t size_in_messages = in_messages_->Get_Size();
	
	if (size_in_messages > 0)
	{
		for (std::size_t j = 0; j < size_in_messages; j++)
		{
			std::shared_ptr<std::string> in_message =
					in_messages_->Pop_Front();
			mavros_msgs:: CommandInt command_msg;
			unsigned int command = 0;

			sscanf(in_message->c_str(), "%u", &command);

			if (static_cast<unsigned int>(mavros_msgs::CommandCode::NAV_WAYPOINT) == command)
			{
				Waypoint_S new_waypoint;
				sscanf(in_message->c_str(), "%u %u %u %lf %u %u ",
					&command, &new_waypoint.latitude,
					&new_waypoint.longitude, &new_waypoint.altitude,
					&new_waypoint.staytime, &new_waypoint.heading);
				command_msg.request.x = new_waypoint.latitude;
				command_msg.request.y = new_waypoint.longitude;
				command_msg.request.z = new_waypoint.altitude;
				// TO DO add staytime and heading;
			}

			command_msg.request.command = command;
			
			if (mav_dji_client_.call(command_msg))
				ROS_INFO("XBeeMav: Command Successfully Tranferred to Dji Mav.");
			else
				ROS_INFO("XBeeMav: Faild to Tranfer Command.");
			
		}
	}
}


//*****************************************************************************
inline void CommunicationManager::Send_Mavlink_Message_Callback(
		const mavros_msgs::Mavlink::ConstPtr& mavlink_msg)
{
	const unsigned short MAX_BUFFER_SIZE = 211; /* 20 (length(uint64_t)) * 10 (max int number) + 10 (spaces) + 1 */
	const unsigned short MAX_NBR_OF_INT64 = 20;
	char temporary_buffer[MAX_BUFFER_SIZE];
	std::string frame;
	int converted_bytes = 0;

	/* Check the payload is not too long. Otherwise ignore it. */
/*	if (mavlink_msg->payload64.size() <= MAX_NBR_OF_INT64)
	{
		for (unsigned short i = 0; i < mavlink_msg->payload64.size(); i++)
		{
			converted_bytes += sprintf(
				temporary_buffer + converted_bytes, "%" PRIu64 " ",
				(uint64_t)mavlink_msg->payload64.at(i));
		
		}
	
		Generate_Transmit_Request_Frame(temporary_buffer, &frame);
		serial_device_.Send_Frame(frame);
	}
	else
	{*/
		char temporary_buffer_check[6000];
		for(int i=0; i<mavlink_msg->payload64.size(); i++)
		{
			converted_bytes += sprintf(
				temporary_buffer_check + converted_bytes, "%" PRIu64 " ",
				(uint64_t)mavlink_msg->payload64.at(i));
			
		}
		frame.append(temporary_buffer_check, std::strlen(temporary_buffer_check));
		uint16_t check_sum = (uint16_t)Caculate_Checksum(&frame);
		uint16_t cnt=0;
		uint16_t number=1;
		uint16_t total =ceil((double)((double)mavlink_msg->payload64.size()/(double)10));
		std::cout <<"Payload size" <<mavlink_msg->payload64.size() << std::endl;
		uint64_t header = (uint64_t)0 | ((uint64_t)check_sum << 16) | ((uint64_t)number << 32) |((uint64_t) total << 48) ;
		std::cout << "Total chunks:" <<check_sum << std::endl;
		//temporary_buffer[MAX_BUFFER_SIZE]="";
		frame="";
		uint16_t* header_16 = u64_cvt_u16(header);
		std::cout << "Sent header" <<header_16[0]<<"  "<<header_16[1]<<"  "<<header_16[2]<<"  "<<header_16[3]<<"  "<< std::endl;
		converted_bytes= sprintf(
				temporary_buffer,  "%" PRIu64 " ",
				(uint64_t)header);	
		delete[] header_16;
		for (int i =0; i<mavlink_msg->payload64.size(); i++)
		{
			
			if(cnt<10){
				converted_bytes += sprintf(
				temporary_buffer+converted_bytes, "%" PRIu64 " ",
				(uint64_t)mavlink_msg->payload64.at(i));
				cnt++;
			}	
			if(cnt==10)
			{	std::cout << "Multi frame sent no:"<<number << std::endl;
				Generate_Transmit_Request_Frame(temporary_buffer, &frame);
				serial_device_.Send_Frame(frame);
				number++;
				cnt=0;
				frame = "";
				std::cout << "checksum:" <<total << std::endl;
				header=0;
				header = 0 | ((uint64_t)check_sum << 16) | ((uint64_t)number << 32) |((uint64_t) total << 48) ;
				header_16 = u64_cvt_u16(header);				
				std::cout << "Sent header" <<header_16[0]<<"  "<<header_16[1]<<"  "<<header_16[2]<<"  "<<header_16[3]<<"  "<< std::endl;
				delete[] header_16;
				memset(temporary_buffer, 0, MAX_BUFFER_SIZE);
				converted_bytes = sprintf(
						temporary_buffer, "%" PRIu64 " ",
						(uint64_t)header);
	
					
			}
		}
                if(total==1){
		std::cout << "Single frame" << std::endl;
		Generate_Transmit_Request_Frame(temporary_buffer, &frame);
				serial_device_.Send_Frame(frame);
		}
		if(number==total){
		 Generate_Transmit_Request_Frame(temporary_buffer, &frame);
				serial_device_.Send_Frame(frame);
		}
		
	//}
}


//*****************************************************************************
void CommunicationManager::Display_Drone_Type_and_Running_Mode(
		DRONE_TYPE drone_type, RUNNING_MODE running_mode)
{
	if (DRONE_TYPE::MASTER == drone_type)
		std::cout << "Drone: MASTER" << std::endl;
	else
		std::cout << "Drone: SLAVE" << std::endl;

	if (RUNNING_MODE::SOLO == running_mode)
		std::cout << "XBeeMav Running in SOLO Mode..." << std::endl;
	else
		std::cout << "XBeeMav Running in SWARM Mode..." << std::endl;
}


}


}
