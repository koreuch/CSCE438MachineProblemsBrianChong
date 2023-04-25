#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <string>
#include <unistd.h>
#include <grpc++/grpc++.h>
#include "client.h"
#include<glog/logging.h>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#define log(severity, msg) LOG(severity) << msg; google::FlushLogFiles(google::severity); 

#include "sns.grpc.pb.h"
#include "snsCoordinator.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
using csce438::Message;
using csce438::ListReply;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;

using snsCoordinator::SNSCoordinator;
using snsCoordinator::user_info;
using snsCoordinator::server_info;
using snsCoordinator::ServerType;
using snsCoordinator::ClusterId;
using snsCoordinator::Users;
using snsCoordinator::FollowSyncs;
using snsCoordinator::Heartbeat;

std::string cport; // just so I can have it whenever

std::unordered_map<std::string, std::string> followerBank = {};

Message MakeMessage(const std::string& username, const std::string& msg) {
    Message m;
    m.set_username(username);
    m.set_msg(msg);
    google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
    timestamp->set_seconds(time(NULL));
    timestamp->set_nanos(0);
    m.set_allocated_timestamp(timestamp);
    return m;
}

int vectorContains(std::vector<std::string> vec, std::string c){
    for (std::string a : vec){
        if (a == c){
            return 1;
        }
    }
    return 0;
}

class Client : public IClient
{
    public:
        Client(const std::string& hname,
               const std::string& uname,
               const std::string& p)
            :hostname(hname), username(uname), port(p)
            {}
        void updated_run();
    protected:
        virtual int connectTo();
        virtual int connectToCoordinator(){return 1;};
        virtual void sendHeartBeats(){};
        virtual IReply processCommand(std::string& input);
        virtual void processTimeline();
        virtual void getForeignClients(std::string uname);
        std::unique_ptr<SNSCoordinator::Stub> c_stub;

    private:
        std::string hostname;
        std::string username;
        std::string port;
        std::string coordinatorPort;
        // You can have an instance of the client stub
        // as a member variable.
        std::unique_ptr<SNSService::Stub> stub_;
        std::string foreignClients = ""; // to not cause a bug
        std::vector<std::string> fc_vector = {};

        IReply Login();
        IReply List();
        IReply Follow(const std::string& username2);
        IReply UnFollow(const std::string& username2);
        void Timeline(const std::string& username);
        void getSlavePort(Heartbeat* hb);

    void display_title() const
    {
        std::cout << "\n========= TINY SNS CLIENT =========\n";
        std::cout << " Command Lists and Format:\n";
        std::cout << " FOLLOW <username>\n";
        std::cout << " UNFOLLOW <username>\n";
        std::cout << " LIST\n";
        std::cout << " TIMELINE\n";
        std::cout << "=====================================\n";
    }

    std::string get_command() const
    {
        std::string input;
        while (1) {
            std::cout << "Cmd> ";
            std::getline(std::cin, input);
            std::size_t index = input.find_first_of(" ");
            if (index != std::string::npos) {
                std::string cmd = input.substr(0, index);
                to_upper_case(cmd);
                if(input.length() == index+1){
                    std::cout << "Invalid Input -- No Arguments Given\n";
                    continue;
                }
                std::string argument = input.substr(index+1, (input.length()-index));
                input = cmd + " " + argument;
            } else {
                to_upper_case(input);
                if (input != "LIST" && input != "TIMELINE") {
                    std::cout << "Invalid Command\n";
                    continue;
                }
            }
            break;
        }
        return input;
    }

    void to_upper_case(std::string& str) const
    {
        std::locale loc;
        for (std::string::size_type i = 0; i < str.size(); i++)
            str[i] = toupper(str[i], loc);
    }



    void display_command_reply(const std::string& comm, const IReply& reply) const
    {
        if (reply.grpc_status.ok()) {
            switch (reply.comm_status) {
                case SUCCESS:
                    std::cout << "Command completed successfully\n";
                    if (comm == "LIST") {
                        std::cout << "All users: ";
                        for (std::string room : reply.all_users) {
                            std::cout << room << ", ";
                        }
                        // appending the foreign clients that were received from the 
                        // call getforeignclients in the ::list command
                        std::cout << foreignClients << std::endl; // just appending the list of foreign clients here
                        std::cout << "\nFollowers: ";

                        for (std::string room : reply.followers) {
                            std::cout << room << ", ";
                        }
                        // eventually we can appending the string of this clients followers later on
                        std::cout << std::endl;
                        
                    }
                    break;
                case FAILURE_ALREADY_EXISTS:
                    std::cout << "Input username already exists, command failed\n";
                    // std::cout << "I'm guessing the error comes froms this" << std::endl;

                    break;
                case FAILURE_NOT_EXISTS:
                    std::cout << "Input username does not exists, command failed\n";
                    break;
                case FAILURE_INVALID_USERNAME:
                    std::cout << "Command failed with invalid username\n";
                    break;
                case FAILURE_INVALID:
                    std::cout << "Command failed with invalid command\n";
                    break;
                case FAILURE_UNKNOWN:
                    std::cout << "Command failed with unknown reason\n";
                    break;
                default:
                    std::cout << "Invalid status\n";
                    break;
            }
        } else {
            // here we can do a heartbeat to the coordinator to get the slave port
            std::cout << "grpc failed: " << reply.grpc_status.error_message() << std::endl;
        }
    }




};

int main(int argc, char** argv) {

    std::string hostname = "localhost";
    std::string username = "default";
    std::string port = "3010";
    

    // the username will always be an int for this implementation
    int opt = 0;
    while ((opt = getopt(argc, argv, "h:u:p:")) != -1){
        switch(opt) {
            case 'h':
                hostname = optarg;break;
            case 'u':
                username = optarg;break;
            case 'p':
                port = optarg;break;
            default:
                std::cerr << "Invalid Command Line Argument\n";
        }
    }
    std::string log_file_name = std::string("client-") + username;
    google::InitGoogleLogging(log_file_name.c_str());
    log(INFO, "Logging Initialized. Client starting...");
    Client myc(hostname, username, port);
    // You MUST invoke "run_client" function to start business logic
    myc.updated_run();

    return 0;
}

void Client::getForeignClients(std::string uname){
    Request request;
    request.set_username(uname);

    Reply reply;
    ClientContext context;

    Status status = stub_->getForeignClients(&context, request, &reply);

    // setting foriegnclient data memeber the list of all clients that are from differnt
    // clusters and the clients you follow from different clusters
    foreignClients = reply.msg();

    std::stringstream ss(foreignClients);

    std::string c;
    std::cout << foreignClients << "there should be no new line here" << std::endl;
    while (std::getline(ss,c,',')){
        std::string addition = "";
        for (char ch : c){
            if (ch != ' '){
                addition += ch;
            }
        }

        if (addition != ""){
            fc_vector.push_back(addition); // it's messy to deal with white space issues
        }
    }

    for (std::string ci : fc_vector){
        std::cout << "Added this " << ci << std::endl;
    }

}




void Client::updated_run()
{
//connectocoordinator done here first
    coordinatorPort = port;
    grpc::ClientContext context;
    std::string login_info = hostname + ":" + port;
    c_stub = std::unique_ptr<SNSCoordinator::Stub>(SNSCoordinator::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));

    google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
    timestamp->set_seconds(time(NULL));
    timestamp->set_nanos(0);
    // std::cout << "We reached here in sendHeartBerats" << std::endl;
    Heartbeat heartBeat;
    heartBeat.set_server_id(std::stoi(username)); // we can have the id as an argument later
    heartBeat.set_server_ip(hostname); // this is all being done on one machine anways
    heartBeat.set_server_port("-1"); // the heartbeat we are sending has no port, but the heartbeat sent back will have a 
    // port no for the coordinator to send back. if the port is -1, then the coordinator knows it's a request from the client
    heartBeat.set_allocated_timestamp(timestamp);
    Heartbeat heartBeatReceived;

    std::shared_ptr<ClientReaderWriter<Heartbeat, Heartbeat>> stream(c_stub->HandleHeartBeats(&context));

    stream->Write(heartBeat);
    stream->Read(&heartBeatReceived);
    // we send a single heartbeat and get back a heartbeat with server ip information
    
    // std::cout << "Reached here at least" << heartBeatReceived.server_port() << std::endl;

    port = heartBeatReceived.server_port();
    // the port that is used in connectTo will now be the one from the heartbeat

// coordinator logic end





    int ret = connectTo(); // we're setting up a connection with the coordinator first
    if (ret < 0) {
        std::cout << "connection failed: " << ret << std::endl;
        exit(1);
    }
    // std::cout << "now you have to coordinate with the server" << std::endl;
    display_title();
    while (1) {
        std::string cmd = get_command();
        IReply reply = processCommand(cmd);
        display_command_reply(cmd, reply);
        if (reply.grpc_status.ok() && reply.comm_status == SUCCESS
                && cmd == "TIMELINE") {
            std::cout << "Now you are in the timeline" << std::endl;
            processTimeline();
        }
        else if (!reply.grpc_status.ok()){
            std::cout << "Command not able to get through, re-routing you to different server" << std::endl;
            Heartbeat *hb = new Heartbeat; // it might be bad practice, whatever it's not working right now
            getSlavePort(hb);
            std::string login_info = hostname + ":" + hb -> server_port(); /// try the literal port when failing
            // std::cout << "The stub is created at the beginning without doing anything" << std::endl;
            stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));    
            // retry again after this        

            //this is just loggin in again to another server
            Request request;
            request.set_username(username);
            Reply reply;
            ClientContext context;
            Status status = stub_->Login(&context, request, &reply);
        }
    }
}






int Client::connectTo()
{
	// ------------------------------------------------------------
    // In this function, you are supposed to create a stub so that
    // you call service methods in the processCommand/porcessTimeline
    // functions. That is, the stub should be accessible when you want
    // to call any service methods in those functions.
    // I recommend you to have the stub as
    // a member variable in your own Client class.
    // Please refer to gRpc tutorial how to create a stub.
	// ------------------------------------------------------------
    // std::cout << "WHy is this not showing up" << std::endl;
    std::string login_info = hostname + ":" + port;
    // std::cout << "get rid of this port is " << port << std::endl;
    // std::cout << "The stub is created at the beginning without doing anything" << std::endl;
    stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));

    IReply ire = Login();
    if(!ire.grpc_status.ok()) {
        return -1;
    }
    // std::cout << "ConnectTODONE" << std::endl;
    return 1;
}


void Client::getSlavePort(Heartbeat* hb)
{
    // I can just call this again at any time to get a heartbeat back. This part of
    // the code is present in the updated_run part, but I don't want to spend the time
    // to abstract it out
    grpc::ClientContext context;
    std::string login_info = hostname + ":" + coordinatorPort;
    c_stub = std::unique_ptr<SNSCoordinator::Stub>(SNSCoordinator::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));

    google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
    timestamp->set_seconds(time(NULL));
    timestamp->set_nanos(0);
    // std::cout << "We reached here in sendHeartBerats" << std::endl;
    Heartbeat heartBeat;
    heartBeat.set_server_id(std::stoi(username)); // we can have the id as an argument later
    heartBeat.set_server_ip(hostname); // this is all being done on one machine anways

    // the -2 here is to let the heartbeat handler in the coordinator know that we want a slave to handle the requests for us
    heartBeat.set_server_port("-2"); // the heartbeat we are sending has no port, but the heartbeat sent back will have a 
    // port no for the coordinator to send back. if the port is -1, then the coordinator knows it's a request from the client
    heartBeat.set_allocated_timestamp(timestamp);
    Heartbeat heartBeatReceived;

    std::shared_ptr<ClientReaderWriter<Heartbeat, Heartbeat>> stream(c_stub->HandleHeartBeats(&context));

    stream->Write(heartBeat);
    stream->Read(&heartBeatReceived);
    hb -> set_server_port(heartBeatReceived.server_port());; // we can provide a heartbeat and then get a heartbeat returned to us 
    // with server info
}



IReply Client::processCommand(std::string& input)
{
	// ------------------------------------------------------------
	// GUIDE 1:
	// In this function, you are supposed to parse the given input
    // command and create your own message so that you call an 
    // appropriate service method. The input command will be one
    // of the followings:
	//
	// FOLLOW <username>
	// UNFOLLOW <username>
	// LIST
    // TIMELINE
	//
	// - JOIN/LEAVE and "<username>" are separated by one space.
	// ------------------------------------------------------------
	
    // ------------------------------------------------------------
	// GUIDE 2:
	// Then, you should create a variable of IReply structure
	// provided by the client.h and initialize it according to
	// the result. Finally you can finish this function by returning
    // the IReply.
	// ------------------------------------------------------------
    
    
	// ------------------------------------------------------------
    // HINT: How to set the IReply?
    // Suppose you have "Join" service method for JOIN command,
    // IReply can be set as follow:
    // 
    //     // some codes for creating/initializing parameters for
    //     // service method
    //     IReply ire;
    //     grpc::Status status = stub_->Join(&context, /* some parameters */);
    //     ire.grpc_status = status;
    //     if (status.ok()) {
    //         ire.comm_status = SUCCESS;
    //     } else {
    //         ire.comm_status = FAILURE_NOT_EXISTS;
    //     }
    //      
    //      return ire;
    // 
    // IMPORTANT: 
    // For the command "LIST", you should set both "all_users" and 
    // "following_users" member variable of IReply.
	// ------------------------------------------------------------
    IReply ire;
    std::size_t index = input.find_first_of(" ");
    log(INFO, "Processing "+input);
    if (index != std::string::npos) {
        std::string cmd = input.substr(0, index);


        /*
        if (input.length() == index + 1) {
            std::cout << "Invalid Input -- No Arguments Given\n";
        }
        */

        std::string argument = input.substr(index+1, (input.length()-index));

        if (cmd == "FOLLOW") {
            return Follow(argument);
        } else if(cmd == "UNFOLLOW") {
            return UnFollow(argument);
        }
    } else {
        if (input == "LIST") {
            return List();
        } else if (input == "TIMELINE") {
            ire.comm_status = SUCCESS;
            return ire;
        }
    }

    ire.comm_status = FAILURE_INVALID;
    return ire;
}

void Client::processTimeline()
{
    Timeline(username);
	// ------------------------------------------------------------
    // In this function, you are supposed to get into timeline mode.
    // You may need to call a service method to communicate with
    // the server. Use getPostMessage/displayPostMessage functions
    // for both getting and displaying messages in timeline mode.
    // You should use them as you did in hw1.
	// ------------------------------------------------------------

    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    //
    // Once a user enter to timeline mode , there is no way
    // to command mode. You don't have to worry about this situation,
    // and you can terminate the client program by pressing
    // CTRL-C (SIGINT)
	// ------------------------------------------------------------
}






IReply Client::List() {
    //Data being sent to the server
    // getForeignClients(username); // this will set the foreignClients private data member 

    Request request;
    request.set_username(username);

    //Container for the data from the server
    ListReply list_reply;

    //Context for the client
    ClientContext context;

    Status status = stub_->List(&context, request, &list_reply);
    IReply ire;
    ire.grpc_status = status;
    //Loop through list_reply.all_users and list_reply.following_users
    //Print out the name of each room
    if(status.ok()){
        ire.comm_status = SUCCESS;
        std::string all_users;
        std::string following_users;
        for(std::string s : list_reply.all_users()){
            ire.all_users.push_back(s);
        }
        for(std::string s : list_reply.followers()){
            ire.followers.push_back(s);
        }
    }

    ClientContext context1;
    Reply reply;
    Status status1 = stub_->getForeignClients(&context1, request, &reply);
    foreignClients = reply.msg();
    return ire;
}
        
IReply Client::Follow(const std::string& username2) {
    Request request;
    request.set_username(username);
    request.add_arguments(username2);

    Reply reply;
    ClientContext context;

    Status status = stub_->Follow(&context, request, &reply);

    IReply ire; ire.grpc_status = status;
    if (reply.msg() == "unknown user name") {
        ire.comm_status = FAILURE_INVALID_USERNAME;
    } else if (reply.msg() == "unknown follower username") {
        ire.comm_status = FAILURE_INVALID_USERNAME;
    } else if (reply.msg() == "Join Failed -- Already Following User") {
        ire.comm_status = FAILURE_ALREADY_EXISTS;
    } else if (reply.msg() == "Join Successful") {
        ire.comm_status = SUCCESS;
    } else {
        ire.comm_status = FAILURE_UNKNOWN;
    }
    return ire;
    // }
}

IReply Client::UnFollow(const std::string& username2) {
    Request request;

    request.set_username(username);
    request.add_arguments(username2);

    Reply reply;

    ClientContext context;

    Status status = stub_->UnFollow(&context, request, &reply);
    IReply ire;
    ire.grpc_status = status;
    if (reply.msg() == "unknown follower username") {
        ire.comm_status = FAILURE_INVALID_USERNAME;
    } else if (reply.msg() == "you are not follower") {
        ire.comm_status = FAILURE_INVALID_USERNAME;
    } else if (reply.msg() == "UnFollow Successful") {
        ire.comm_status = SUCCESS;
    } else {
        ire.comm_status = FAILURE_UNKNOWN;
    }

    return ire;
}

IReply Client::Login() {
    Request request;
    request.set_username(username);
    Reply reply;
    ClientContext context;

    Status status = stub_->Login(&context, request, &reply);

    IReply ire;
    ire.grpc_status = status;
    if (reply.msg() == "you have already joined") {
        ire.comm_status = FAILURE_ALREADY_EXISTS;
    } else {
        ire.comm_status = SUCCESS;
    }
    return ire;
}

void Client::Timeline(const std::string& username) {
    ClientContext context;
    std::cout << "Testing here " << std::endl;
    std::shared_ptr<ClientReaderWriter<Message, Message>> stream(
            stub_->Timeline(&context));
    //Thread used to read chat messages and send them to the server
    std::thread writer([username, stream]() {
            std::string input = "Set Stream";
            Message m = MakeMessage(username, input);
            stream->Write(m);
            while (1) {
            input = getPostMessage();
            m = MakeMessage(username, input);
            stream->Write(m);
            }
            stream->WritesDone();
            });

    std::thread reader([username, stream]() {
            Message m;
            while(stream->Read(&m)){

            google::protobuf::Timestamp temptime = m.timestamp();
            std::time_t time = temptime.seconds();
            displayPostMessage(m.username(), m.msg(), time);
            }
            });

    //Wait for the threads to finish
    writer.join();
    reader.join();
}
