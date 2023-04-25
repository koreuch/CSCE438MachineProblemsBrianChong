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

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include<glog/logging.h>
#include <thread>
#include <unordered_set>
#include <mutex>
#include <sys/stat.h>

#define log(severity, msg) LOG(severity) << msg; google::FlushLogFiles(google::severity); 

#include "snsCoordinator.grpc.pb.h"
#include "sns.grpc.pb.h"
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


using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReader;
using grpc::ClientReaderWriter;
using grpc::ClientWriter;
using grpc::Status;
// having to add these in now since this process now communicates to server via client


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
// using snsCoordinator::MASTER;
// using snsCoordinator::SLAVE;
// using snsCoordinator::SYNC;

std::mutex foreign_client_lock;

struct timespec fcTime;

std::unique_ptr<Server> server; // I included this here 
// for testing purposes, I comment this out later

std::string ID; // use this to get the server id at any time

std::unordered_map<std::string, std::string> followerBank = {};

int vectorContains(std::vector<std::string> vec, std::string c){
    for (std::string a : vec){
        if (a == c){
            return 1;
        }
    }
    return 0;
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

std::string vecToString(std::vector<std::string> v){
    std::string returnString = "";

    for (std::string s : v){
        returnString += (s + "+");
    }
    return returnString;
}

std::string vecToStringPretty(std::vector<std::string> v){
    std::string returnString = "";

    for (std::string s : v){
        returnString += (s + ", ");
    }
    return returnString;
}

int updateFollowers(std::string path, std::string uname, std::string myname){
  // apply the lock here
  std::string foreign_path = "foreignClients_" + ID + ".txt";
  std::ifstream fc(foreign_path);

  // I guess so don't read in anything if there is nothing to read in
  std::stringstream foreignClients;
  foreignClients << fc.rdbuf();
  fc.close();
  // give back the lock to coordinator
  // std::cout << "The client we are trying to add is " << myname << std::endl;
  // std::cout << "my file path is " << path << std::endl;
  int edited = -1;

  std::string addition;
  std::vector<std::string> fc_list;
  while(std::getline(foreignClients,addition,'+')){
        fc_list.push_back(addition);
  }// end while
  // just getting all the clinets that we don't have on the cluster

// yeah that is wrong


//MAJOR STEPS
// read from foreignClients_n 
// then WRITE to followers_n. at present i'm writing and reading to the same file, unacceptable

// the problem is all in this crappy funciton here

  // why is followers_ foreign clients
  int lineNum = 0;
  std::string editedLine;
  for (std::string follower_list : fc_list){
    
    std::vector<std::string> followers = splitString(follower_list);
    if (followers.front() == uname){
      if (vectorContains(followers, myname)){
        return -1;
      }
// here we update the list if the vector doesn't contain the follower
      editedLine = (follower_list + myname + "+");
      std::cout << "Follower list is now " << follower_list << std::endl;
      edited = 1;
      break;
    } // checking that the front of the list is who we want to follow
    lineNum += 1;
  }/// end for

  int currentNum = 0;
  if (edited == 1){
  // request lock now for file overwriting
    std::ofstream following_file(path,std::ios::trunc|std::ios::out|std::ios::in);
    for (std::string follower_list : fc_list){
      
      if (currentNum != lineNum){
        following_file << (follower_list + "\n");
      }
      else{
        following_file << (editedLine + "\n");
        std::cout << "Editd line is " << editedLine << std::endl;
      }

      currentNum += 1;
    }
    following_file.close();
  /// send back the lock 

  }
  else{ //edited is -1, so nobody follows this foreign client yet
  //request the lock now
    std::ofstream following_file(path,std::ios::app|std::ios::out|std::ios::in);
    following_file << (uname + "+" + myname + "+" + "\n");
    following_file.close();
    std::cout << "This is the one that should appear first when there is no file" << std::endl;
    // send back the lock now
  }
  return 1;
}




struct Client {
  std::string username;
  bool connected = true;
  int following_file_size = 0;
  std::vector<Client*> client_followers;
  std::vector<Client*> client_following;
  std::vector<std::string> foreign_client_followers;
  std::vector<std::string> foreign_client_following;
  ServerReaderWriter<Message, Message>* stream = 0;
  bool operator==(const Client& c1) const{
    return (username == c1.username);
  }
};

// this class is meant for interfacing with the coordinator from the client. There were
//scoping issues before when just using "class Client : public IClient"
class serverClient : public IClient{ 
    public:
        serverClient(const std::string& hname,
               const std::string& uname,
               const std::string& p, 
               const std::string& t,
               const std::string& sp)
            :hostname(hname), username(uname), port(p), server_type(t), server_port(sp)
            {}
        void sendBeats(){
          sendHeartBeats();
        }

        void changeType(std::string type){
          server_type = type;
        }

        virtual ~serverClient(){}

    protected:
        virtual int connectTo();
        virtual IReply processCommand(std::string& input);
        virtual void processTimeline();
        virtual int connectToCoordinator();
        virtual void sendHeartBeats();

        std::string hostname;
        std::string username;
        std::string port;
        std::string server_type;
        std::string server_port;
        // You can have an instance of the client stub
        // as a member variable.
        std::unique_ptr<SNSService::Stub> stub_;
        std::unique_ptr<SNSCoordinator::Stub> c_stub;

        IReply Login();
        IReply GetServer();
        IReply List();
        IReply Follow(const std::string& username2);
        IReply UnFollow(const std::string& username2);
        void Timeline(const std::string& username);
};
IReply serverClient::GetServer() {

    IReply ire;  
    return ire;
}

int serverClient::connectToCoordinator()  
{
    return 1;
}

int serverClient::connectTo()  
{
    return 1;
}

void serverClient::sendHeartBeats() {
    grpc::ClientContext context;
    std::string login_info = hostname + ":" + port;

    google::protobuf::Timestamp* timestamp = new google::protobuf::Timestamp();
    timestamp->set_seconds(time(NULL));
    timestamp->set_nanos(0);
    // std::cout << "We reached here in sendHeartBerats" << std::endl;
    Heartbeat heartBeat;
    heartBeat.set_server_id(std::stoi(username)); // we can have the id as an argument later

    if (server_type == "master"){
    heartBeat.set_server_type(snsCoordinator::MASTER); // this means master, we can set that up for an argument later
    }
    else if (server_type == "slave"){
      heartBeat.set_server_type(snsCoordinator::SLAVE); // this means master, we can set that up for an argument later
    }
    heartBeat.set_server_ip(hostname); // this is all being done on one machine anways
    // std::cout << "The port is " << port << std::endl;
    heartBeat.set_server_port(server_port); // a default value that we can set an argument for later
    heartBeat.set_allocated_timestamp(timestamp);
    // std::cout << "We reached this after" << std::endl;
    // std::cout << "Just checking that the server type was changed" << heartBeat.server_type() << std::endl;

    c_stub = std::unique_ptr<SNSCoordinator::Stub>(SNSCoordinator::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));



    std::shared_ptr<ClientReaderWriter<Heartbeat, Heartbeat>> stream(c_stub->HandleHeartBeats(&context));
    /// now do a loop here

    
    while(stream -> Write(heartBeat)){

      // std::cout << "This should update every 10 seconds" << std::endl;
      sleep(10);
      google::protobuf::Timestamp* newTimestamp = new google::protobuf::Timestamp();
      newTimestamp->set_seconds(time(NULL));
      newTimestamp->set_nanos(0);
      heartBeat.set_allocated_timestamp(newTimestamp);
      // heartBeat.set_server_id(1); // we can have the id as an argument later

      //this is for if the server type is ever changed given that the master server goes down
      if (server_type == "master"){
      heartBeat.set_server_type(snsCoordinator::MASTER); // this means master, we can set that up for an argument later
      }
      else if (server_type == "slave"){
        heartBeat.set_server_type(snsCoordinator::SLAVE); // this means master, we can set that up for an argument later
      }
      // heartBeat.set_server_ip(hostname); // this is all being done on one machine anways
      // heartBeat.set_server_port(port); // a default value that we can set an argument for later
      google::protobuf::Timestamp temptime = heartBeat.timestamp();
      std::time_t ttime = temptime.seconds();
      // std::cout << "The servertype is " << heartBeat.server_type() << std::endl;
      // displayPostMessage(std::to_string(heartBeat.server_id()), std::to_string(heartBeat.server_type()), ttime);

    }//end while
    std::cout << "reached this point" << std::endl;

    return;


}

IReply serverClient::processCommand(std::string& input){
  IReply ire;
  return ire;
}

void serverClient::processTimeline(){
  //do nothing // this is for the client 
}
// int serverClient::connectToCoordinator(){
//   // do nothing, it's for the client
//   return 1;
// }








//Vector that stores every client that has been created
std::vector<Client> client_db;
std::vector<std::string> foreign_db; // these are usernames of clients not on this cluster

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

class SNSServiceImpl final : public SNSService::Service { // update this so that it receives a pointer to the
//coordinator client as well

  public:
    SNSServiceImpl(serverClient* heartBeatSender): heartBeatSender(heartBeatSender){}
  
  private:
    serverClient* heartBeatSender;
    std::string foreign_client_list;

  Status changeServerType(ServerContext* context, ServerReaderWriter<Request, Request>* stream) override {
    Request r;
    stream->Read(&r);
    log(INFO,"Serving ServerType Change Request");
    // this is responsible for changing the slave to a master
    heartBeatSender -> changeType("master");
    stream->Write(r);
    return Status::OK; 
  }

  Status List(ServerContext* context, const Request* request, ListReply* list_reply) override {
    log(INFO,"Serving List Request");
    Client user = client_db[find_user(request->username())];
    int index = 0;
    for(Client c : client_db){
      list_reply->add_all_users(c.username);
    }
    //testing this out, remove later
    // list_reply->add_all_users("8");


    // list_reply->add_followers("89");
    //testing ended


    std::vector<Client*>::const_iterator it;
    for(it = user.client_followers.begin(); it!=user.client_followers.end(); it++){
      list_reply->add_followers((*it)->username);
    }

    // now read in all the foreign clients so that they can be given to the client


    std::string fileName = "foreignClients_" + ID + ".txt";
    // apply lock here
    std::ifstream fc(fileName);
    std::stringstream foreign_file_contents;
    foreign_file_contents << fc.rdbuf();
    fc.close();
    // relinquish lock here i guess
    std::string foreign_string = foreign_file_contents.str();
    std::vector<std::string> client_vec = splitString(foreign_string);
    sort( client_vec.begin(), client_vec.end() );
    client_vec.erase( unique( client_vec.begin(), client_vec.end() ), client_vec.end() );
    std::string sorted_string = vecToStringPretty(client_vec);
    foreign_client_list = sorted_string;




    return Status::OK;
  }

  Status sendFollows(ServerContext* context, const Request* request, Reply* reply) override {
    log(INFO,"letting master know which non native clusters to send followers");


    
    return Status::OK; 
  }


  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    log(INFO,"Serving Follow Request");
    std::string username1 = request->username();
    std::string username2 = request->arguments(0);
    int join_index = find_user(username2);

    // i use these for the foreign cluster list access
    int cluster = std::stoi(username2);
    cluster = cluster % 3;

    if (std::to_string(cluster) != ID){

    /// here we deal with foreign clients
      std::string foreign_clients = "foreignClients_" + ID + ".txt";
      struct stat modTime;
      if (stat(foreign_clients.c_str(), &modTime) == 0){
        if ((modTime.st_mtim.tv_sec != fcTime.tv_sec) || (modTime.st_mtim.tv_nsec != fcTime.tv_nsec)){
          // apply the lock here
          std::ifstream fc(foreign_clients);
          std::stringstream foreignClients;
          foreignClients << fc.rdbuf();
          // we need to add unlock here too

          std::string fc_string = foreignClients.str();
          std::vector<std::string> fc_list = splitString(fc_string);

          if (vectorContains(fc_list, username2)){
            // if the list of foreign clients contains what we need, then we can proceed
            std::string followerFilePath = "followers_" + ID + ".txt"; // this file has a list of foreign clients
            // that our native clients follow and who follows who            
            int updated = updateFollowers(followerFilePath, username2, username1);
            if (updated == 1){
              reply->set_msg("Join Successful");
            }
            else{
              reply->set_msg("Join Failed -- Already Following User");
            }
          } // if the file we read in of foreign clients even has the client in question
          else{
            reply->set_msg("unknown user name");
          }
        }
        else{
          // file hasn't been updated yet so we don't have to read form it
          reply->set_msg("unknown user name");
        }
      }// if an actual foreign clients file even exists


      else{
        // followers file doesn't exist so we don't do anything
        reply->set_msg("unknown user name");
      }


    } // this if is if we're outside the cluster 



  else{
      if(join_index < 0 || username1 == username2)
        reply->set_msg("unknown user name");
      else{
        Client *user1 = &client_db[find_user(username1)];
        Client *user2 = &client_db[join_index];
        if(std::find(user1->client_following.begin(), user1->client_following.end(), user2) != user1->client_following.end()){
            reply->set_msg("Join Failed -- Already Following User");
          return Status::OK;
        }
        user1->client_following.push_back(user2);
        user2->client_followers.push_back(user1);
        reply->set_msg("Join Successful");
      }
  }
    return Status::OK; 
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
    log(INFO,"Serving Unfollow Request");
    std::string username1 = request->username();
    std::string username2 = request->arguments(0);
    int leave_index = find_user(username2);
    if(leave_index < 0 || username1 == username2)
      reply->set_msg("Leave Failed -- Invalid Username");
    else{
      Client *user1 = &client_db[find_user(username1)];
      Client *user2 = &client_db[leave_index];
      if(std::find(user1->client_following.begin(), user1->client_following.end(), user2) == user1->client_following.end()){
	reply->set_msg("Leave Failed -- Not Following User");
        return Status::OK;
      }
      user1->client_following.erase(find(user1->client_following.begin(), user1->client_following.end(), user2)); 
      user2->client_followers.erase(find(user2->client_followers.begin(), user2->client_followers.end(), user1));
      reply->set_msg("Leave Successful");
    }
    return Status::OK;
  }
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    // std::cout << "HEY I'm here" << std::endl;
    log(INFO,"Serving Login Request");
    Client c;
    std::string username = request->username();
    int user_index = find_user(username);
    if(user_index < 0){
      c.username = username;
      client_db.push_back(c);
      reply->set_msg("Login Successful!");
    }
    else{ 
      Client *user = &client_db[user_index];
      if(user->connected)
        reply->set_msg("Invalid Username");
      else{
        std::string msg = "Welcome Back " + user->username;
	reply->set_msg(msg);
        user->connected = true;
      }
    }
// here we add to the list of clients that are in this cluster



// ID is a global variable here

    std::string local_clients = "local_clients_" + ID + ".txt";
    // we want to add a client to the file each time that one login so the syncer can send this
    // info to other files
    std::ofstream client_file(local_clients,std::ios::app|std::ios::out|std::ios::in);
    client_file << (username + "+");
    std::cout << "Adding to the file, local clients" << std::endl;

    client_file.close();
/// give back the file lock


    return Status::OK;
  }

  Status getForeignClients(ServerContext* context, const Request* request, Reply* reply) override {

    reply -> set_msg(foreign_client_list);
    return Status::OK;
  }

  Status getServerClients(ServerContext* context, const Request* request, Reply* reply) override {
    //yet to implement
    // std::cout << "Reached getSErverClients" << std::endl;
    std::string listOfMyClients = "";
    for (Client c: client_db){
      listOfMyClients += (c.username + "+");
    }
    reply -> set_msg(listOfMyClients);
    return Status::OK;
  }



  Status Timeline(ServerContext* context, 
		ServerReaderWriter<Message, Message>* stream) override {
    log(INFO,"Serving Timeline Request");
    Message message;
    Client *c;
    while(stream->Read(&message)) {
      std::string username = message.username();
      int user_index = find_user(username);
      c = &client_db[user_index];
 
      //Write the current message to "username.txt"
      std::string filename = username+".txt";
      std::ofstream user_file(filename,std::ios::app|std::ios::out|std::ios::in);
      google::protobuf::Timestamp temptime = message.timestamp();
      std::string time = google::protobuf::util::TimeUtil::ToString(temptime);
      std::string fileinput = message.username()+ " - " + time+" :: "+message.username()+":"+message.msg()+"\n";
      //"Set Stream" is the default message from the client to initialize the stream

      //this was just me testing how I would read the files
      // std::stringstream ss;
      // ss << user_file.rdbuf();status
      // std::string tester;

      // while (std::getline(ss,tester,'\n')){

      //   if (tester == ""){
      //     continue;
      //   }
      //   std::stringstream temp(tester);
      //   std::string asdf;
      //   std::getline(ss,asdf,'-');
      //   std::cout << "ID" << std::stoi(asdf) << "ID" << std::endl;

      //   std::cout << tester << std::endl;
      // }

      // for (auto x: client_db){
      //   std::cout << x.username << std::endl;
      //   std::cout << "nothing here" << std::endl;
      // }
      // this is just more testing to see how I can do the database

      // std::istream_iterator<std::string> begin(ss);
      // std::istream_iterator<std::string> end;
      // std::vector<std::string> writtenMessages(begin, end);
      // std::copy(writtenMessages.begin(), writtenMessages.end(), std::ostream_iterator<std::string>("\n"));


      // std::vector<std::string> betterVector = splitString(tester);
      // for (auto x: betterVector){
      //   std::cout << x << " here is a message" << std::endl;
      // }

// std::istream_iterator<std::string> begin(ss);
// std::istream_iterator<std::string> end;
// std::vector<std::string> vstrings(begin, end);
// std::copy(vstrings.begin(), vstrings.end(), std::ostream_iterator<std::string>(std::cout, "\n"));





      if(message.msg() != "Set Stream")
        user_file << fileinput;
      //If message = "Set Stream", print the first 20 chats from the people you follow
      else{
        if(c->stream==0)
      	  c->stream = stream;
        std::string line;
        std::vector<std::string> newest_twenty;
        std::ifstream in(username+"following.txt");
        int count = 0;
        //Read the last up-to-20 lines (newest 20 messages) from userfollowing.txt
        while(getline(in, line)){
          if(c->following_file_size > 20){
	    if(count < c->following_file_size-20){
              count++;
	      continue;
            }
          }
          newest_twenty.push_back(line);
        }
        Message new_msg; 
 	//Send the newest messages to the client to be displayed
	for(int i = 0; i<newest_twenty.size(); i++){
	  new_msg.set_msg(newest_twenty[i]);
          stream->Write(new_msg);
        }    
        continue;
      }
      //Send the message to each follower's stream
      std::vector<Client*>::const_iterator it;
      for(it = c->client_followers.begin(); it!=c->client_followers.end(); it++){
        Client *temp_client = *it;
      	if(temp_client->stream!=0 && temp_client->connected)
	  temp_client->stream->Write(message);
        //For each of the current user's followers, put the message in their following.txt file
        std::string temp_username = temp_client->username;
        std::string temp_file = temp_username + "following.txt";
	std::ofstream following_file(temp_file,std::ios::app|std::ios::out|std::ios::in);
	following_file << fileinput;
        temp_client->following_file_size++;
	std::ofstream user_file(temp_username + ".txt",std::ios::app|std::ios::out|std::ios::in);
        user_file << fileinput;
      }
    }
    //If the client disconnected from Chat Mode, set connected to false
    c->connected = false;
    return Status::OK;
  }

};

void RunServer(std::string port_no, serverClient* heartBeatSender) {
  std::string server_address = "0.0.0.0:"+port_no;
  SNSServiceImpl service(heartBeatSender);

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server = (builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  log(INFO, "Server listening on "+server_address);

  server->Wait();
}

void coordinatorThreadFunction(serverClient* heartBeatSender){
  heartBeatSender->sendBeats();
}

int main(int argc, char** argv) {


  
  std::string serverName = "server1";
  std::string CoordinatorPort = "3010";
  std::string CoordinatorIp = "3010";
  std::string  serverId = "1";
  std::string serverType = "master"; //0, 1, and 2 for master, slave, sync



  std::string port = "3010";

  fcTime.tv_nsec = 0;
  fcTime.tv_sec = 0;
  
  int opt = 0; 
  // get opt takes single character flags, so these were modified from how they appeared in the file
  while ((opt = getopt(argc, argv, "p:c:i:I:t:")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;break;
      case 'c':
          CoordinatorPort = optarg;break;
      case 'i':
          CoordinatorIp = optarg;break;
      case 'I':
          serverId = optarg;break;
      case 't':
          serverType = optarg;break;
      default:
	  std::cerr << "Invalid Command Line Argument\n";
    }
  }

  ID = serverId;

  serverClient* heartBeatSender = new serverClient("localhost", serverId, CoordinatorPort, serverType, port);
  // heartBeatSender.sendBeats(); // just public acessor for send heartbeats
  std::thread heartBeatThread(coordinatorThreadFunction, heartBeatSender);
  RunServer(port, heartBeatSender);
  heartBeatThread.join();
  server -> Shutdown(); // comment this out when done with testing. Right now
  // the server has global scope since it makes it easier to kill all the processes for retestingzs
  std::cout << "Reached this point after all the heart beats were sent" << std::endl;
  
    std::string log_file_name = std::string("server-") + port;
    google::InitGoogleLogging(log_file_name.c_str());
    log(INFO, "Logging Initialized. Server starting...");
  // RunServer(port);

  return 0;
}
