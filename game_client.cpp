// Karen Wong
// CPSC3500
// 6/1/2020
// game_client.cpp

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <iostream>
#include <arpa/inet.h>

using namespace std;

const int NUM_OF_WORDS = 57127;
const int LEN_BUFFER = 1000;


unsigned short get_port_num(int argc, char* argv[]);
void sendString(int socket, string str);
string receive(int socket);
void playGame(int socket, string username);

int main(int argc, char* argv[])
{
  struct sockaddr_in serverAddr;
  if(argc != 3){
    cout << "Error: Insufficient number of inputs -- IP address and/or port number is missing."
         << endl;
    exit(1);
  }

  int ip_addr = inet_pton(AF_INET,argv[1],&(serverAddr.sin_addr.s_addr));
  if(ip_addr == 0 || ip_addr == -1){
    cout << "Error: IP address is invalid!" << endl;
    exit(1);
  }

  unsigned short portNum = get_port_num(argc,argv);

  // create socket
  int client_socket = socket(AF_INET, SOCK_STREAM, 0);

  // server address
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(portNum);

  int value = connect(client_socket,(struct sockaddr*)&serverAddr,sizeof(serverAddr));

  if(value == -1){
    cout << "Error: Connection failed." << endl;
    close(client_socket);
    exit(1);
  }

  string username = "";
  cout << "Welcome to Hangman!" << endl;
  cout << "Enter username: ";
  cin >> username;
  sendString(client_socket,username); // send #1 -- the username

  playGame(client_socket,username);
  close(client_socket);

  return 1;
}

unsigned short get_port_num(int argc, char* argv[])
{
  if(argc != 3){
    cout << "Error: Insufficient number of inputs -- IP address and/or port number is missing."
         << endl;
    exit(1);
  }
  short portNum;
  portNum = atoi(argv[2]);
  if(portNum == 0 || portNum > 65535){
    cout << "Error: Port number is invalid!" << endl;
    exit(1);
  }
  return portNum;
}

void sendString(int socket, string str)
{
  int length = str.length()+1;
  char buffer[length];
  strcpy(buffer, str.c_str());
  buffer[length] = '\0';
  while(length > 0){
    int sendStat = send(socket, buffer, length, 0);
    if(sendStat == -1){
      cout << "Error: Send failed." << endl;
      close(socket);
      exit(1);
    }
    length -= sendStat;
    assert(length >= 0);
  }
}

string receive(int socket)
{
  char buffer[LEN_BUFFER];
  bool done = false;
  while(!done){
    int recv_bytes = recv(socket, buffer, LEN_BUFFER, 0);
    if(recv_bytes == 0){
      cout << "Error: Server has closed the connection unexpectedly." << endl;
      exit(1);
    }
    if(recv_bytes == -1){
      cout << "Error: Receive failed." << endl;
      exit(1);
    }
    for(int i = 0; !done && i < recv_bytes; i++){
      if(buffer[i] == '\0')
        done = true;
    }
  }
  return buffer;
}

void playGame(int socket, string username)
{
  uint16_t valid;
  uint16_t correct;
  uint16_t done;
  int recvStat;
  string guess;
  string response;
  bool guessed = false;

  while(!guessed){
    response = receive(socket); // recv #1 -- # of turns & blanks
    cout << response;
    cout << "Enter your guess: ";
    cin >> guess;
    while(guess.length() != 1 || isalpha(guess[0]) == 0){
      cout << "\nGuess is invalid. Please try again!" << endl;
      cout << "Enter your guess: ";
      cin >> guess;
    }
    sendString(socket,guess);
    
    recvStat = recv(socket,&valid,sizeof(uint16_t),0); // recv #2 -- validity
    if(recvStat == -1){
      cout << "Error: Receive valid failed." << endl;
      close(socket);
      exit(1);
    }

    // check validity
    if(ntohs(valid) == 0){
      cout << "You had already tried that character -- Please try again!" << endl;
      cout << "You will be penalized." << endl;
    }
    else{
      recvStat = recv(socket,&correct,sizeof(uint16_t),0); // recv #3 -- correctness
      if(recvStat == -1){
        cout << "Error: Receive correctness failed." << endl;
        close(socket);
        exit(1);
      }
      // check correctness
      if(ntohs(correct) == 0)
        cout << "Incorrect!" << endl;
      else
        cout << "Correct!" << endl;
      
      recvStat = recv(socket, &done, sizeof(uint16_t),0); // recv #4 -- if done
      if(recvStat == -1){
        cout << "Error: Receive doneness failed." << endl;
        close(socket);
        exit(1);
      }
      
      if(ntohs(done) == 1){
        guessed = true;
        response = receive(socket); // recv #5 -- congratulatory message + leaderboard
        cout << response << endl;
      }
      else
        guessed = false;
    }
  }
}
