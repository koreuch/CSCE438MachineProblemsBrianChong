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
#include <sys/types.h>
#include <sys/stat.h>
#include <experimental/filesystem>
#include <fstream>

namespace fs = std::experimental::filesystem;

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

time_t foreignClientModTime = 0;
time_t followerModTime = 0;


std::vector<std::string> foreignClients = {};
std::mutex client_lock;

std::string ID; 

std::string vecToString(std::vector<std::string> v){
    std::string returnString = "";

    for (std::string s : v){
        returnString += (s + "+");
    }
    return returnString;
}

// std::vector<std::string> serializeFile(){

// }




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
    
    int id = std::stoi(ID);

    std::string fileName = "foreignClients_";
    if (id % 3 == 0){
        fileName += "0.txt";
    }
    else if (id % 3 == 1){
        fileName += "1.txt";
    }
    else{
        fileName += "2.txt";
    }

    struct stat modTime;

    std::string client_list = request -> username(); // not the username, but the list of clients from a differnt cluster
    // std::vector<std::string> client_vector = splitString(client_list);
    // // client_lock.lock();

    // // checking here in locked section that we can add clients to synchronizers db
    // if (!foreignClients.empty()){
    //     for (int i = 0; i < client_vector.size(); ++i){
    //         int found = 0;
    //         for (int j = 0; j < foreignClients.size(); ++j){
    //             if (foreignClients[j] == client_vector[i]){
    //                 found = 1;
    //             }
    //         }
    //         if (found == 0){
    //             foreignClients.push_back(client_vector[i]);
    //         }
    //     }
    // }
    // else{
    //     foreignClients = client_vector;
    // }
    // client_lock.unlock();

    // // std::cout << "Sync id is " << ID << std::endl; // for testin gpurposes delte later
    // // for (std::string newClient: foreignClients){
    // //     std::cout << std::endl;
    // //     std::cout << newClient << std::endl;
    // //     std::cout << "checking " << std::endl;
    // // }
    
    std::string local_clients = "local_clients_" + ID + ".txt";
    // we want to add a client to the file each time that one login so the syncer can send this
    // info to other files
    std::ofstream foreign_clients_file(fileName,std::ios::app|std::ios::out|std::ios::in);
    foreign_clients_file << client_list;
    std::cout << "Adding to the foreign file" << std::endl;

    foreign_clients_file.close();

    // we send the lock back to the coordinator here


    return Status::OK;
  }
  Status sendClientUpdates(ServerContext* context, const Request* request, Reply* reply) override {

    return Status::OK;
  }



};

void runSyncService(std::string port_no){
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

    ID = syncId; // this is for testing purposes, comment  out later

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

    std::cout << "reached inside the sync function, this is happening for a reason you aren't crazy" << std::endl;

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
    std::cout << "reached inside the sync function, this is happening for a reason you aren't crazy pt 16" << std::endl;

    while(1){
        // this process shouldn't stop
    }

    std::shared_ptr<ClientReaderWriter<Heartbeat, Heartbeat>> stream(c_stub->HandleHeartBeats(&context));
    std::vector<std::string> syncPorts;
    int retrievedAllSyncPorts = 0;
    while (!retrievedAllSyncPorts){
        std::cout << "Have not yet received all ports for " << syncId << std::endl;
        stream->Write(hb);
        stream->Read(&HB);
        if (HB.server_port() == "-1"){
            sleep(.5);
            continue;
        }
        else{
            syncPorts = splitString(HB.server_port()); 
            retrievedAllSyncPorts = 1;
        }
    } // end while
    // now we have all the information we need to update other syncers of any new info

        std::cout << "reached inside the sync function, this is happening for a reason you aren't crazy pt2" << std::endl;

    std::vector<std::string> other_synchronizers;
    if (syncId == "0"){
        other_synchronizers.push_back(syncPorts[1]);
        other_synchronizers.push_back(syncPorts[2]);
    }
    else if (syncId == "1"){
        other_synchronizers.push_back(syncPorts[0]);
        other_synchronizers.push_back(syncPorts[2]);
    }
    else{
        other_synchronizers.push_back(syncPorts[0]);
        other_synchronizers.push_back(syncPorts[1]);
    }

    struct timespec last_mod;
    last_mod.tv_nsec = 0;
    last_mod.tv_sec = 0;



    std::cout << "we aren't reaching here 0" << std::endl;

    while(1){

        /// here we should obtain the key first before we write to the file

// ID is a global variable here

    std::string local_clients = "local_clients_" + syncId + ".txt";
    struct stat modTime;
    if (stat(local_clients.c_str(), &modTime) == 0){
        // this means that we should read stuff in

        if ((last_mod.tv_sec != modTime.st_mtim.tv_sec) || (last_mod.tv_nsec != modTime.st_mtim.tv_nsec)){
            std::cout << "change was made to the file since last time" << std::endl;

            // APPLY THE LOCK HERE BEFORE TIMESTAMPS APPLIED

            std::string local_clients = "local_clients_" + syncId + ".txt";
            // we need to request a lock here remember that 

            std::ifstream client_file(local_clients);
            std::stringstream newClients;
            newClients << client_file.rdbuf();

            client_file.close();
            // now here is where we give back the lock

            std::string new_clients = newClients.str();

            last_mod.tv_nsec = modTime.st_mtim.tv_nsec;
            last_mod.tv_sec = modTime.st_mtim.tv_sec;
            
            for (std::string port : other_synchronizers){
                ClientContext context;
                login_info = hostname + ":" + port;
                stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));

                Request rq;
                Reply reply;
                rq.set_username(new_clients);
                Status status = stub_->sendListOfClients(&context, request, &reply);



            }

            // now we send the stuff over



        }
    }
    else{
        // don't do anything, the file wasn't made yet
        std::cout << "modtime isn't being written to " << std::endl;
        std::cout << last_mod.tv_nsec << std::endl;
        std::cout << last_mod.tv_sec << std::endl;
    }





        sleep(5); // we run this loop every 30 seconds for syncing

    }
}