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
#include <unordered_map>

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



std::vector<std::string> splitString(std::string stringList){
    std::vector<std::string> returnStrings;

    std::stringstream stream(stringList);
    std::string addition;
    while(std::getline(stream,addition,'+')){
        returnStrings.push_back(addition);
    }
    return returnStrings;
}


std::string vecToString(std::vector<std::string> v){
    std::string returnString = "";

    for (std::string s : v){
        returnString += (s + "+");
    }
    return returnString;
}


std::vector<std::string> fetch_follower_list(std::string id){
    std::string followerFilePath = "followers_" + ID + ".txt";
    std::ifstream client_file(followerFilePath);
    std::stringstream fc;
    fc << client_file.rdbuf();
    client_file.close();

    std::string follower_line;
    while(std::getline(fc, follower_line, '\n')){
        std::stringstream line(follower_line);
        std::string client_num;
        std::getline(line, client_num, '+');
        if (client_num == id){
            std::vector<std::string> followers = splitString(follower_line);
            followers.erase(followers.begin());
            return followers;
        }
    }
    std::vector<std::string> empty_vec;
    return empty_vec;
}


class syncService final : public SNSService::Service{ // update this so that it receives a pointer to the
//coordinator client as well

  public:
    syncService(){}
  
  private:

  Status sendFollows(ServerContext* context, const Request* request, Reply* reply) override {

    std::string file_name = "following_" + ID + ".txt";
    std::vector<std::string> list_of_follows;
    std::stringstream ss(request->username());
    std::string line;
    std::unordered_map<std::string, std::string> client_set;
    
    while (std::getline(ss,line, '\n' )){
        std::stringstream candidate(line);
        std::string cand_string;
        std::getline(candidate, cand_string, '+');
        if (std::stoi(cand_string) % 3 == std::stoi(ID)){
            client_set.insert({cand_string, line});
        }
    } // here we are only adding clients that are followed if they match our cluster id

    std::string new_followers = "";
    struct stat mt;
    std::unordered_map<std::string, std::string> client_copy;
    for (auto x: client_set){
        client_copy.insert({x.first, x.second});
    }


    std::string payload = "";

    if (stat(file_name.c_str(), &mt) == 0){
        // file exists so we write to it
        std::ifstream f(file_name);
        std::stringstream f1;
        f1 << f.rdbuf();
        f.close();
        std::string f_line;
        while (std::getline(f1, f_line, '\n')){
            std::stringstream client_finder(f_line);
            std::string client_id;
            std::getline(client_finder, client_id, '+');
            if (!client_set.count(client_id)){
                payload += (f_line + "\n");
            }
            else if (client_set.count(client_id)){
                std::cout << "------------" << std::endl;

                std::cout << "FLINE" << f_line << std::endl;
                std::cout << "Read in client map: " << client_set.at(client_id);
                std::vector<std::string> vec1 = splitString(f_line);
                std::vector<std::string> vec2 = splitString(client_set.at(client_id));
                vec1.erase(vec1.begin());
                vec2.erase(vec2.begin());
                client_copy.erase(client_id);

                std::unordered_set<std::string> combo;
                for (auto x: vec1){
                    if(!combo.count(x)){
                        combo.insert(x);
                    }
                }
                for (auto x: vec2){
                    if(!combo.count(x)){
                        combo.insert(x);
                    }
                } // do this to combine the elements that are different in each of the respective vectors
                
                std::string follower_line = "";
                for (auto i = combo.begin(); i != combo.end(); i++){
                    follower_line +=  (*i + "+");
                }
                follower_line = (client_id + "+" + follower_line);
                std::cout << "Followerline: " << follower_line << std::endl;
                std::cout << "------------" << std::endl;
                payload += (follower_line + "\n");
            }
        }
        for (auto x : client_copy){
            payload += (x.second + "\n");
        }

        // any elements in the map that weren't deleted from also being
        // in the original file are added here
        std::ofstream client_file(file_name,std::ios::trunc|std::ios::out|std::ios::in);
        client_file << payload;
        client_file.close();

    }
    else{
        // here we just straight up make a file to write to
        std::string inputString = "";
        for (auto i = client_set.begin(); i != client_set.end(); i++){
            inputString += (i -> second + "\n");
        }
        std::cout << inputString << std::endl;
        if (inputString != ""){
            std::ofstream client_file(file_name,std::ios::trunc|std::ios::out|std::ios::in);
            client_file << inputString;
            client_file.close();
        }
        // if you write to file with an empty string, it throws everything off
    }

    // give back lock here
    return Status::OK;
  }








  Status getServerClients(ServerContext* context, const Request* request, Reply* reply) override {

    return Status::OK;
  }
  Status sendListOfClients(ServerContext* context, const Request* request, Reply* reply) override {
 
    
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
    std::string client_list = request -> username(); // not the username, but the list of clients from a differnt cluster

    std::string local_clients = "local_clients_" + ID + ".txt";
    // we want to add a client to the file each time that one login so the syncer can send this
    // info to other files
    std::ofstream foreign_clients_file(fileName,std::ios::app|std::ios::out|std::ios::in);
    foreign_clients_file << client_list;

    foreign_clients_file.close();

    // we send the lock back to the coordinator here


    return Status::OK;
  }
  Status sendClientUpdates(ServerContext* context, const Request* request, Reply* reply) override {

    std::string updates = request -> username();


    std::stringstream update_list(updates);

    std::string line;
    std::string prev_string = "nothing";

    std::string follower_string;
    std::vector<std::string> followers;

    // request the lock here

    while(std::getline(update_list, line, '\n')){
        // these two strings mark the start of a client's batch of updates
        std::cout << "The line that is being iterated over is " << line << std::endl;
        if ((prev_string == "nothing") || (prev_string == "+")){
            followers = fetch_follower_list(line);
            std::cout << "This should be just a number: " << line << std::endl;
        }
        else if (line == "+"){
            // do nothing, it's just a separator between batches
        }
        else{
            std::cout << "THere just aren't any memebers in followers for some reason" << std::endl;
            // here we get a single line and send it to all the necessary files
            for (std::string id : followers){
                // std::cout << "The follower we are updateing is " << id << std::endl;
                std::string file_path = id + ".txt";
                // std::cout << "File path is " << file_path << std::endl;

                std::string follower_file_path = id + "following.txt";
                // std::cout << "File path is " << follower_file_path << std::endl;
                // we don't have to check for the file's existence here, we can just write to it

                std::ofstream fp(file_path,std::ios::app|std::ios::out|std::ios::in);
                fp << (line + "\n\n");
                fp.close();

                std::ofstream ff(follower_file_path,std::ios::app|std::ios::out|std::ios::in);
                ff << (line + "\n\n");
                ff.close();
            }
        }
        prev_string = line;
    }

    // return lock here 
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
    std::vector<std::string> syncPorts;
    int retrievedAllSyncPorts = 0;
    while (!retrievedAllSyncPorts){
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



    std::string list_of_clients;
    std::unordered_map<std::string, struct timespec> timeStamps;
    std::unordered_map<std::string, std::string> lastLines;

    std::unordered_map<std::string, struct timespec> followerTimeStamps;

    struct timespec follower_mod;
    follower_mod.tv_nsec = 0;
    follower_mod.tv_sec = 0;
    while(1){

        /// here we should obtain the key first before we write to the file

// ID is a global variable here

    std::string local_clients = "local_clients_" + syncId + ".txt";
    struct stat modTime;

    if (stat(local_clients.c_str(), &modTime) == 0){
        // this means that we should read stuff in

        if ((last_mod.tv_sec != modTime.st_mtim.tv_sec) || (last_mod.tv_nsec != modTime.st_mtim.tv_nsec)){
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

                list_of_clients = new_clients;
                
                Status status = stub_->sendListOfClients(&context, rq, &reply);
            }
            // now we send the stuff over
        }
    }
    else{
        // don't do anything, the file wasn't made yet

    }


    // now looking at local files and seeing which ones 

        std::vector<std::string> client_vec = splitString(list_of_clients);

        sort( client_vec.begin(), client_vec.end() );
        client_vec.erase( unique( client_vec.begin(), client_vec.end() ), client_vec.end() );

        // should just ask for the lock here since it'll probably be easier to deal with 

        std::string payload = "";

        for (std::string c : client_vec){
            std::string fileName = c + ".txt";
            if (stat(fileName.c_str(), &modTime) == 0){
                if (!timeStamps.count(c)){
                    struct timespec c_mod;
                    c_mod.tv_nsec = 0;
                    c_mod.tv_sec = 0;
                    timeStamps.insert({c, c_mod});
                    lastLines.insert({c, "nothing"}); // no line will have nothing as it's text
                    // std::cout << "Added eerything we need for " << fileName << std::endl;
                } // checking if it's already in the timestamp, if 
                // not we make a timestamp object and put it in
                std::string addition;
                // to be used with the getline below

                if ((modTime.st_mtim.tv_nsec != timeStamps.at(c).tv_nsec) || (modTime.st_mtim.tv_sec != timeStamps.at(c).tv_sec)){
                    payload += (c + "\n");
                    // an edit was made so we should write the edits to the other synchronizers
                    std::ifstream timeline(fileName);
                    std::stringstream timelineStream;
                    timelineStream << timeline.rdbuf();
                    timeline.close();
                    
                    int readNewLines = 0;

                    std::string lastLine = "nothing";

                    while(std::getline(timelineStream,addition,'\n')){

                        if (addition == lastLines.at(c)){
                            readNewLines = 1;
                            continue;
                        } // this is so i don't read in old lines

                        if (readNewLines == 1 || lastLines.at(c) == "nothing"){
                            if (addition != ""){
                                std::stringstream s(addition);
                                std::string num;
                                std::getline(s, num, '-');
                                num.erase(std::remove(num.begin(),num.end(),' '),num.end());

                                if (num == c){
                                    // std::cout << "add line to the payload: " << addition << std::endl;
                                    payload += (addition + "\n");
                                    lastLine = addition;
                                }
                            }
                        }
                    }// end while
                    if (lastLine != "nothing"){
                        lastLines.at(c) = lastLine;
                    } 
                    // std::cout << "lastlines is " << lastLines.at(c) << std::endl;
                    payload += ("+\n");

                } // if no modification time change, then don't update the timelines when sending stuff
            }
            else{
            }
            // don't do anything in the else case, the timeline file doesn't exist
        }
        // std::cout << "HEre is the payload: " << std::endl;
        // std::cout << payload << std::endl;
        // // relinquish the lock here after we read through all the necessary files

        // // here we send the file updates to other synchronizers

        for (std::string port : other_synchronizers){
            ClientContext context;
            login_info = hostname + ":" + port;
            stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
                grpc::CreateChannel(
                login_info, grpc::InsecureChannelCredentials())));

            Request rq;
            Reply reply;
            rq.set_username(payload);            
            Status status = stub_->sendClientUpdates(&context, rq, &reply);
        }

        std::string file_name = "followers_" + syncId + ".txt";

        struct stat m;
        if (stat(file_name.c_str(), &m) == 0){
            if ((m.st_mtim.tv_nsec != follower_mod.tv_nsec) || (m.st_mtim.tv_sec != follower_mod.tv_sec)){
                // mod times differ so we can proceed
                
                // request lock here
                std::ifstream f(file_name);
                std::stringstream followers;
                followers << f.rdbuf();
                f.close();
                follower_mod.tv_nsec = m.st_mtim.tv_nsec;
                follower_mod.tv_sec = m.st_mtim.tv_sec;
                // give up lock here
                
                for (std::string port : other_synchronizers){
                    ClientContext context2;
                    login_info = hostname + ":" + port;
                    stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
                        grpc::CreateChannel(
                        login_info, grpc::InsecureChannelCredentials())));

                    Request rq;
                    Reply reply;
                    rq.set_username(followers.str());  
                    Status status = stub_->sendFollows(&context2, rq, &reply);
                }
            }
        }
        else{
            // do nothing, file doesn't exist so no need for rpc calls yet
        }



        // this section will handle writing to other synchronizers follow records

        // plan
         // read the followers_n.txt file to a string stream
         // divide the lines in the files into two strings, one for each of the 
         // the foreign clusters
         // send the payloads in rpc
            // - on the server side, they will reference this in the instance of a list command from the client


        sleep(30); // we run this loop every 30 seconds for syncing

}

}