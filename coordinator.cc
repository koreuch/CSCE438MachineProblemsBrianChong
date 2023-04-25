/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <mutex>
#include <fstream>
#include <iostream>
#include <thread>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include<glog/logging.h>
#include<map>
#define log(severity, msg) LOG(severity) << msg; google::FlushLogFiles(google::severity); 

// #include "sns.grpc.pb.h"
#include "sns.grpc.pb.h"
#include "snsCoordinator.grpc.pb.h"
#include "client.h"


using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;

using grpc::ClientReaderWriter;

using csce438::Message;
using csce438::ListReply;
using csce438::Request;
// using csce438::Reply;
using csce438::SNSService;
using snsCoordinator::user_info;
using snsCoordinator::server_info;
using snsCoordinator::Heartbeat;
using snsCoordinator::SNSCoordinator;



// std::pair<int, int>f = {0, 0};
// std::pair<int, int>s = {0, 0};
// std::pair<int, int>t = {0, 0};

int heartBeatCount = 0;

std::map<std::string, std::string> foreign_followers1;
std::map<std::string, std::string> foreign_followers2;
std::map<std::string, std::string> foreign_followers3;

std::vector<std::string> clients1;
std::vector<std::string> clients2;
std::vector<std::string> clients3;


// std::string asdf = foreign_followers1["a"];

std::vector<std::string> syncPorts = {"nothing", "nothing", "nothing"};

int vectorContains(std::vector<std::string> vec, std::string c){
    for (std::string a : vec){
        if (a == c){
            return 1;
        }
    }
    return 0;
}

// just taking the class from the tsc.cc file and defining fxn's with nonsense implemntations for time
class coordinatorClient : public IClient
{
    public:
        coordinatorClient(const std::string& hname,
               const std::string& uname,
               const std::string& p)
            :hostname(hname), username(uname), port(p)
            {}
        void checkHeartBeats(){};
    protected:
        virtual int connectTo(){return 1;};
        virtual IReply processCommand(std::string& input){IReply ir; return ir;};
        virtual void processTimeline(){};
        virtual int connectToCoordinator(){return 1;};
        virtual void sendHeartBeats(){};
    private:
        std::string hostname;
        std::string username;
        std::string port;
        std::string server_type = 0;
        // You can have an instance of the client stub
        // as a member variable.
        std::unique_ptr<SNSService::Stub> stub_;
        std::unique_ptr<SNSCoordinator::Stub> c_stub;

        IReply Login(){IReply ir; return ir;};
        IReply GetServer(){IReply ir; return ir;};
        IReply List(){IReply ir; return ir;};
        IReply Follow(const std::string& username2){IReply ir; return ir;};
        IReply UnFollow(const std::string& username2){IReply ir; return ir;};
        void Timeline(const std::string& username){};


};

struct Client {
  std::string username;
  bool connected = true;
  int following_file_size = 0;
  std::vector<Client*> client_followers;
  std::vector<Client*> client_following;
  ServerReaderWriter<Message, Message>* stream = 0;
  bool operator==(const Client& c1) const{
    return (username == c1.username);
  }
};

//Vector that stores every client that has been created
std::vector<Client> client_db;

//Helper function used to find a Client object given its username
int find_user(std::string username){
  int index = 0;
  for(Client c : client_db){
    if(c.username == username)
      return index;
    index++;
  }
  return -1;
}

class SNSCoordinatorImpl : public SNSCoordinator::Service {
  
  // int listOfServers;

  public:
  SNSCoordinatorImpl(std::vector<std::pair<std::string, std::string>>* serverList,
                    std::vector<std::pair<int, int>>* ts,  std::vector<coordinatorClient*>& cc) 
                    : listOfServers(serverList), timeStamps(ts),  coordinatorClients(cc){}
  

  private:
  std::vector<std::pair<std::string, std::string>>* listOfServers; // will keep track of the servers for routin gpurposese
  std::vector<std::pair<int, int>>* timeStamps;
  std::vector<coordinatorClient*> coordinatorClients;


  Status GetServer(ServerContext* context, const user_info* user, server_info* server) override {
    std::cout << "Hey we tried GetServer up to this point" << std::endl;

    return Status::OK;
  }




  Status HandleHeartBeats(ServerContext* context, ServerReaderWriter<Heartbeat, Heartbeat>* stream)override {
    // std::cout << "HandleHeartBeats function on coordinator" << std::endl;
    Heartbeat heartBeat;

    // for (int i = 0; i < (*listOfServers).size(); ++i){
    //   std::string n1 = (*listOfServers)[i].first;
    //   std::string n2 = (*listOfServers)[i].second;
    //   std::cout << "Server: " << (*listOfServers)[i].first;
    //   std::cout << "Server: " << (*listOfServers)[i].second;
    //   if (n1 != "nothing" && n2 != "nothing"){
    //     std::cout << "\n-------------Received all the ports-------------\n" << std::endl;
    //   }
    // }
    // std::cout <<std::endl;
    // these std:cout were used for testing purpsoes


     while(stream->Read(&heartBeat)) {
      // std::cout << &timeStamps << " Timestamps here in heartBeatHandler" << std::endl;
      //handling heartbeats from the request handling server
      if (heartBeat.server_port() != "-1" && heartBeat.server_port() != "-2" && 
      heartBeat.server_port() != "-3" && heartBeat.server_port() != "-4"){
        // std::cout << "this should update every 10 seconds" << std::endl;

        
        google::protobuf::Timestamp temptime = heartBeat.timestamp();
        std::time_t time = temptime.seconds();
        // std::cout << "timeT " << time << std::endl;

        if (snsCoordinator::MASTER == heartBeat.server_type()){
          // std::cout << "It's a master server " << std::endl;
          // std::cout << "Type of server is " << heartBeat.server_port() << std::endl;
          (*listOfServers)[heartBeat.server_id() % 3].first = heartBeat.server_port();
          // std::cout << "Now to update time" << std::endl;
          (*timeStamps)[heartBeat.server_id() % 3].first = time;
        }
        else if (snsCoordinator::SLAVE == heartBeat.server_type()){
          // std::cout << "It's a slave server" << std::endl;
          (*listOfServers)[heartBeat.server_id() % 3].second = heartBeat.server_port();
          (*timeStamps)[heartBeat.server_id() % 3].second = time;

        }
        else{
          // std::cout << "It's a sync server" << std::endl;
        }
      }
      //handling heartbeats from the clients to request server infomration for logging in
      else if (heartBeat.server_port() == "-1"){
        int index = heartBeat.server_id() % 3;
        heartBeat.set_server_port((*listOfServers)[index].first); // changing the server id of the heartbeat to the 
        // ip of the master index
        // std::cout << "THe index is " << index << std::endl;
        // std::cout << "heartbeat server port is" << heartBeat.server_port() << std::endl;
        stream->Write(heartBeat);
        //write back the edited heartbeat
      }
      else if (heartBeat.server_port() == "-2"){
        int index = heartBeat.server_id() % 3;
        heartBeat.set_server_port((*listOfServers)[index].second); // changing the server id of the heartbeat to the 
        // ip of the master index
        stream->Write(heartBeat);
        //write back the edited heartbeat
      }
      else if (heartBeat.server_port() == "-3"){
        syncPorts[heartBeat.server_id()] = heartBeat.server_ip();
        // we're sending back multiple ports here
        //here we're just adding the syncport to the list of syncports to be returned to the synchronizers later
        if (vectorContains(syncPorts, "nothing")){
          heartBeat.set_server_port("-1");
          stream->Write(heartBeat);
          // the -1 here will notify the sync process that not all the heartbeats form the
          // syncs have been received, i.e. we don't know all the sync ports 
        }// end if
        else{
        std::string syncPortList = "";
        for (std::string s : syncPorts){
          syncPortList += (s + "+");
        }
        heartBeat.set_server_port(syncPortList);
          stream->Write(heartBeat);
          // here we are sending back the list of syncPOrts as a string

        }//end else
      }
      // else if (heartBeat.server_port() == "-4"){

      //   std::string syncPortList = "";
      //   for (std::string s : syncPorts){
      //     syncPortList += (s + "+");
      //   }
      //   heartBeat.set_server_ip((*listOfServers)[heartBeat.server_id()].first); // this is needed to
      //   //contact our master server for it's clients later
      //   heartBeat.set_server_port(syncPortList);
      //   stream->Write(heartBeat);

      // } // this is outdated functionality
    }




    return Status::OK;
  }


};


void listenForServers(std::string port_no){
  // don't do anything 
  // while (true){
  // std::cout << "Doing Server shit" << std::endl;
  // }
}



void listenForClients(std::string port_no){

}



void RunServer(std::string port_no, std::vector<std::pair<std::string, std::string>>* listOfServers, 
  std::vector<std::pair<int, int>>* timeStamps,  std::vector<coordinatorClient*>* coordinatorClients) {
  std::string server_address = "0.0.0.0:"+port_no;
  SNSCoordinatorImpl service(listOfServers, timeStamps, *coordinatorClients);

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Coorindator listening on " << server_address << std::endl;
  log(INFO, "Coordinator listening on "+server_address);

  server->Wait();
}

void notifySlave(std::string ip, std::string slave_port){
    // std::cout << "Notifying slave to beecome master now" << std::endl;
    std::string login_info = ip + ":" + slave_port;
    grpc::ClientContext context;
    std::unique_ptr<SNSService::Stub> stub = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));


    std::shared_ptr<ClientReaderWriter<Request, Request>> stream(stub->changeServerType(&context));
    Request r;
    stream->Write(r);
    stream->Read(&r);
    // don't move on until we know that the slave has been notified


    // std::shared_ptr<ClientReaderWriter<Request, Request>> stream(stub->changeServerType(&context));

}

void timeCheck(std::vector<std::pair<std::string, std::string>>* listOfServers, 
    std::vector<std::pair<int, int>>* timeStamps,  std::vector<coordinatorClient*>* coordinatorClients){

    while(1){
      sleep(3); // just check to see if there's a lapse in heartbeats once in a while to 
      // not stress the system
      google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
      timestamp->set_seconds(time(NULL));
      timestamp->set_nanos(0); // not sure if this needs to be updated but 
      //just being safe for time
      std::time_t time = timestamp -> seconds();
      delete timestamp;
      int index = 0;
      for (auto x: *timeStamps){ // dereferencing timestamps so we can iterate through them
        if (x.first != 0){ // x.first being 0 means the master server hasn't sent a heartbeat yet
        // since all the values are all intialized to 0
          if (abs(x.first - time) > 25){
            // we need to message the slave to swithch to master here

            // std::cout << "We need to update the stuff here now"<<std::endl;
            std::unique_ptr<SNSService::Stub> c_stub;
            std::string ip = "localhost"; // I'm doing this on one machine.
            std::string slave_port = (*listOfServers)[index].second;

            std::cout << x.first << "  " << time<< std::endl;
            // notifySlave(ip, slave_port);
            std::cout << "We reached this point" << std::endl;
            // notifySlave(ip, slave_port);
            // I could do some stuff to get it dynamically, but I don't want
            //to do that with limited time


          }
          else{
            // std::cout << "the time " << x.first << " is within the time " << time << std::endl;
          }
        }
      // std::cout << "TimeStampCheck " << std::endl;
      // std::cout << &timeStamps << " Timestamps here in timecheck" << std::endl;
      index += 1;
    }
    // for (auto x: *listOfServers){
    //   std::cout << x.first << x.second << std::endl;
    // }


  }
}

int main(int argc, char** argv) {
  std::string port = "3010";
  int opt = 0;
  while ((opt = getopt(argc, argv, "p:")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;break;
      default:
	  std::cerr << "Invalid Command Line Argument\n";
    }
  }

  // just initializing the list of servers that will be used during port routing
  std::pair<std::string, std::string>first = {"nothing", "nothing"};
  std::pair<std::string, std::string> second = {"nothing", "nothing"};
  std::pair<std::string, std::string> third = {"nothing", "nothing"};
  std::vector<std::pair<std::string, std::string>>* listOfServers = new std::vector<std::pair<std::string, std::string>>({first, second, third});
  std::vector<std::pair<int, int>> * timeStamps = new  std::vector<std::pair<int, int>> ({{0, 0},{0, 0},{0, 0}});
  std::vector<coordinatorClient*> * coordinatorClients = new std::vector<coordinatorClient*> ({nullptr, nullptr, nullptr});
  
  std::string log_file_name = std::string("server-") + port;
    google::InitGoogleLogging(log_file_name.c_str());
    log(INFO, "Logging Initialized. Server starting...");
  // RunCoordinator(port);
  // std::cout << &timeStamps << " Timestamps here in main" << std::endl;
  std::thread coordinationThread(RunServer,port, listOfServers, timeStamps, coordinatorClients); // this needs to be threaded, has memory that is hsared
  std::thread timeCheckThread(timeCheck, listOfServers, timeStamps, coordinatorClients);

  timeCheckThread.join();
  coordinationThread.join();
// 
  return 0;
}


// we need two servers at least for the servers coordinatiig and the clients coorindating