#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <string>
#include <unistd.h>
#include <grpc++/grpc++.h>
#include "client.h"
#include<glog/logging.h>
#include<algorithm>
#include <mutex>

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
using grpc::ServerContext;

using snsCoordinator::SNSCoordinator;
using snsCoordinator::user_info;
using snsCoordinator::server_info;
using snsCoordinator::ServerType;
using snsCoordinator::ClusterId;
using snsCoordinator::Users;
using snsCoordinator::FollowSyncs;
using snsCoordinator::Heartbeat;


std::vector<std::string> foreignClients = {};
std::mutex client_lock;


std::string vecToString(std::vector<std::string> v){
    std::string returnString = "";

    for (std::string s : v){
        returnString += (s + "+");
    }
    return returnString;
}



std::vector<std::string> splitString(std::string stringList){
    std::vector<std::string> returnStrings;

    std::stringstream stream(stringList);
    std::string addition;
    while(std::getline(stream,addition,'+')){
        returnStrings.push_back(addition);
    }
    return returnStrings;
}

class syncService final : public SNSService::Service{ // update this so that it receives a pointer to the
//coordinator client as well

  public:
    syncService(){}
  
  private:

  Status getServerClients(ServerContext* context, const Request* request, Reply* reply) override {

    return Status::OK;
  }
  Status sendListOfClients(ServerContext* context, const Request* request, Reply* reply) override {
    // std::cout <<"Want to make sure that this function is running" << std::endl;

    // std::cout << request -> username() << std::endl
    ;
    std::string client_list = request -> username(); // not the username, but the list of clients from a differnt cluster
    std::vector<std::string> client_vector = splitString(client_list);
    client_lock.lock();

    // checking here in locked section that we can add clients to synchronizers db
    if (!foreignClients.empty()){
        for (int i = 0; i < client_vector.size(); ++i){
            int found = 0;
            for (int j = 0; j < foreignClients.size(); ++j){
                if (foreignClients[j] == client_vector[i]){
                    found = 1;
                }
            }
            if (found == 0){
                foreignClients.push_back(client_vector[i]);
            }
        }
    }
    else{
        foreignClients = client_vector;
    }
    client_lock.unlock();

    for (std::string newClient: foreignClients){
        std::cout << newClient << std::endl;
        std::cout << "checking " << std::endl;
    }
    



    return Status::OK;
  }
  Status sendClientUpdates(ServerContext* context, const Request* request, Reply* reply) override {

    return Status::OK;
  }



};

void runSyncService(std::string port_no){
    // std::cout << "is this even running" << std::endl;
    syncService sc;
    std::string server_address = "0.0.0.0:"+port_no;

    grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&sc);
    std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
    log(INFO, "Syncer listening on "+server_address);

    server->Wait();
}


class syncClient : public IClient
{
    public:
        syncClient(const std::string& hname,
               const std::string& syncId,
               const std::string& p)
            :hostname(hname), syncId(syncId), port(p)
            {}
        void updated_run();
        void sync(std::string syncId, std::string port, std::string hostname, std::string syncPort); // this is the only function that i'll actually use

    protected:
        virtual int connectTo(){return 1;};
        virtual int connectToCoordinator(){return 1;};
        virtual void sendHeartBeats(){};
        virtual IReply processCommand(std::string& input){IReply ire; return ire;}
        virtual void processTimeline(){};
        virtual void getForeignClients(std::string uname){};
        std::unique_ptr<SNSCoordinator::Stub> c_stub;

    private:
        std::string hostname;
        std::string syncId;
        std::string port;
        std::string coordinatorPort;
        // You can have an instance of the client stub
        // as a member variable.
        std::unique_ptr<SNSService::Stub> stub_;
        
        // std::string foreignClients;

        IReply Login(){IReply ire; return ire;}
        IReply List(){IReply ire; return ire;}
        IReply Follow(const std::string& username2){IReply ire; return ire;}
        IReply UnFollow(const std::string& username2){IReply ire; return ire;}
        void Timeline(const std::string& username){}
        void getSlavePort(Heartbeat* hb){}


    void display_title() const{}

    std::string get_command() const
    {
     return "nothing";
    }
    void to_upper_case(std::string& str) const
    {
    }
    void display_command_reply(const std::string& comm, const IReply& reply) const
    {}
};



int main(int argc, char** argv) {
    std::string hostname = "localhost";
    std::string syncId = "0";
    std::string port = "3010";
    std::string syncPort = "3020"; // set temorarrily
    

    // the username will always be an int for this implementation

    int opt = 0;
    while ((opt = getopt(argc, argv, "h:i:p:s:")) != -1){
        switch(opt) {
            case 'h':
                hostname = optarg;break;
            case 'i':
                syncId = optarg;break;
            case 'p':
                port = optarg;break;
            case 's':
                syncPort = optarg;break;
            default:
                std::cerr << "Invalid Command Line Argument\n";
        }
    }
    std::string log_file_name = std::string("sync-") + syncId;
    google::InitGoogleLogging(log_file_name.c_str());
    log(INFO, "Logging Initialized. Synchronizer starting...");

    syncClient sc(hostname, syncId, port);
    std::thread syncRequestHandler(runSyncService, syncPort);

    sc.sync(syncId, port, hostname, syncPort);

    syncRequestHandler.join();
    return 0;

}


void syncClient::sync(std::string syncId, std::string port, std::string hostname, std::string syncPort){

// all the client information across servers
    std::vector<std::string> nativeList;


/// end of client information across servers

///find diff will be useful later

    Request request;
    request.set_username(syncId);

    Reply reply;
    ClientContext context;
    ClientContext retrieveSyncPortsContext;

    std::string login_info = hostname + ":" + port;

    // first contact the coordinator 
    Heartbeat hb;
    Heartbeat HB;

    hb.set_server_port("-3");
    hb.set_server_id(std::stoi(syncId)); // to tell the coordinator our id, we'll use this field 
    hb.set_server_ip(syncPort);// we'll use this field to tell the coordinator the port.
    // ^i know that there is a server_port field, but the implementation that exists
    // already from making the coordinator before meant that I don't want to change everything suddently

    c_stub = std::unique_ptr<SNSCoordinator::Stub>(SNSCoordinator::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));



    std::shared_ptr<ClientReaderWriter<Heartbeat, Heartbeat>> stream(c_stub->HandleHeartBeats(&context));
    // std::cout << "We are being blocked here I guess" << std::endl;
    stream->Write(hb);
    stream->Read(&HB);
    sleep(2); // we want to wait to make sure all the synchronizers have 
    // // their ports in the coordiantor
    hb.set_server_port("-4"); // this tells the server that we want to get the other ports for sync
    hb.set_server_id(std::stoi(syncId));; // this will be 0, 1, or 2, and server_ip is not a used field, so we'll use it here 
    // to tell the coordinator our ID
    // std::cout << "before second read";
    stream->Write(hb);
    stream->Read(&HB);
    // std::cout << "after second read" << std::endl;

    // the name for this string is bad, it should be list of syncports, but I don't
    // have time to change it right now
    std::string listOfMasters = HB.server_port();

    // std::cout << "list of masters " << listOfMasters << std::endl;


    std::vector<std::string> portList = splitString(listOfMasters);
    //split string is defined earlier in the file

    std::vector<std::string> foreignPorts;
    std::string nativePort;
    
    nativePort = HB.server_ip(); // using this to get the clients in our cluster. THe ip
    // isn't actually an ip, but a port to the master proces.
    // std::cout << "Native port is " << nativePort << std::endl;

    // std::cout << "There was an error right after this" << std::endl;
    for (int i  = 0; i < portList.size(); ++i){
        std::string x = portList[i];
        // std::cout << "port: " << x << std::endl;
        if (x == "nothing"){
            continue; // a port was not provided by a master server
        }

        // the index is just the cluster that the synchronizer is in, 
        else if (i != (stoi(syncId) % 3)){
            foreignPorts.push_back(x);
        }
    }

    // for (auto str: foreignPorts){
    //     std::cout << "foreignPort " << str << std::endl;
    // }

    


    while (1){
        // sleep(30); // change this to 30 later on
        // the c_stub will give us all the ports we need for the servers                

        // the logic here we need is to supply the stubs with their own login_infos
        ClientContext context;

        login_info = hostname + ":" + nativePort;
        Reply nativeClientReply;
        // std::cout << "Right before the stub" << std::endl;
        stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
                grpc::CreateChannel(
                        login_info, grpc::InsecureChannelCredentials())));

        Status status = stub_ ->getServerClients(&context, request, &nativeClientReply);
        // std::cout << "Our clusters cklients are " << nativeClientReply.msg() << std::endl;



        // sleep(1000);
        for (int i = 0; i < foreignPorts.size(); ++i){
            ClientContext sendListContext;
            ClientContext sendForeignClientsContext;
            ClientContext sendUpdatesContext;

            // sleep (1000); // this is just here temporarily before we implement other stuff

            Request ourClients;
            ourClients.set_username(nativeClientReply.msg());
            // the other synchronizers will access the request's username field
            // which will actually just be a list of usernames that are foreign to that synchronizer

            login_info = hostname + ":" + foreignPorts[i];
            ClientContext context;

            stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));
            // // so that our cluster knows which new clients have since popped up
            // std::cout << "Just making sure this change is maintained throughout " << nativeClientReply.msg() << std::endl;

            Status status = stub_->sendListOfClients(&sendListContext, ourClients, &reply);

            //swithcing back to nativePort in native cluster

            login_info = hostname + ":" + nativePort;

            Request nonNativeClients;
            nonNativeClients.set_username(vecToString(foreignClients));
            stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));

            status = stub_ -> sendServerForeignClients(&sendForeignClientsContext, nonNativeClients, &reply);




            // // so that our cluster with servers that are followers get appropriate updates
            // Status status = s->sendClientUpdates(&context, request, &reply);
        }

        sleep(4);
    }
}