// Karen Wong
// CPSC3500
// 6/1/2020
// game_server.cpp

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>
#include <exception>
#include <iostream>
#include <time.h>
#include <math.h>
#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>

using namespace std;

const int NUM_OF_WORDS = 57127;
const int LEN_BUFFER = 1000;
const int LB_SIZE = 5;

struct player
{
  string name;
  float score;
};

unsigned short get_port_num(int argc, char* argv[]);
void sendString(int socket, string str);
string receive(int socket);
string getWord();
void* startHangman(void* socket);
void sortLeaderBoard();
string formatFloat(float f);

player leaderBoard[LB_SIZE];
pthread_mutex_t m;
string lbHeader = "\n\nLeader board:\n";
int lbEntries = 0;

int main(int argc, char* argv[])
{
  pthread_mutex_init(&m,NULL);
  pthread_t tid;
  
  for(int i = 0; i < 5; i++){
    leaderBoard[i].name = "";
    leaderBoard[i].score = 999999.0;
  }
  
  unsigned short portNum = get_port_num(argc,argv);

  // create server socket
  struct sockaddr_in serverAddr;
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(portNum);
  serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  
  int server_socket = socket(AF_INET,SOCK_STREAM,0);
  if(server_socket == -1){
    cout << "Error: socket" << endl;
    close(server_socket);
    exit(1);
  }
  
  // binding
  int value = bind(server_socket, (struct sockaddr*) &serverAddr,
                   sizeof(serverAddr));
  if(value < 0){
    cout << "Error: binding" << endl;
    exit(1);
  }

  // listening
  value = listen(server_socket,10);
  if(value < 0){
    cout << "Error: listening" << endl;
    exit(1);
  }
  
  // accept & loop
  int client_socket;
  struct sockaddr_in clientAddr;

  while(1){
    socklen_t addr_len = sizeof(clientAddr);
    client_socket = accept(server_socket, (struct sockaddr*) &clientAddr,
                           &addr_len);
    if(client_socket == -1){
      cout << "Error: Client failed to connect!" << endl;
      close(client_socket);
      exit(1);
    }
    cout << "Connection to client ACCEPTED!" << endl;
    
    // create threads
    int val = pthread_create(&tid, NULL, startHangman, (void*)&client_socket);
    if(val != 0){
      cout << "Error: Failed to create thread!" << endl;
      close(client_socket);
      exit(1);
    }
    pthread_detach(pthread_self());
  }
  close(client_socket);
  return 1;
}

unsigned short get_port_num(int argc, char* argv[])
{
  if(argc != 2){
    cout << "Error: Insufficient number of inputs -- Port number is missing."
         << endl;
    exit(1);
  }
  short portNum;
  portNum = atoi(argv[1]);
  if(portNum == 0 || portNum > 65535){
    cout << "Error: Port number is invalid!" << endl;
    exit(1);
  }
  return portNum;
}

string getWord()
{
  srand(time(0));
  string filePath = "/home/fac/lillethd/cpsc3500/projects/networking/words.txt";
  ifstream words_file(filePath);
  int idx = rand() % NUM_OF_WORDS;
  string word = "";
  for(int i = 0; i < idx; i++)
    words_file >> word;
  return word;
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
      pthread_exit(&sendStat);
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
      cout << "Error: Client has closed the connection unexpectedly." << endl;
      pthread_exit(&recv_bytes);
    }
    if(recv_bytes == -1){
      cout << "Error: Receive failed." << endl;
      pthread_exit(&recv_bytes);
    }
    for(int i = 0; !done && i < recv_bytes; i++){
      if(buffer[i] == '\0')
        done = true;
    }
  }
  return buffer;
}

void* startHangman(void* socket)
{
    float score = 0.0;
    string word = "";
    string progress = "";
    string username = "";
    string guess = "";
    string turnAndProgress = "";
    string guessedChars[LEN_BUFFER];
    string response = "";
    string lbRanks = "";
    int guessedCharPtr = 0;
    int word_len;
    int turns = 1;
    int sendVal;
    int correctGuesses = 0;
    int client_socket = *(int*)socket;
    uint16_t valid;
    uint16_t doneSignal = htons(0);
    uint16_t isCorrect;
    bool correct;
    bool done = false;
    bool guessed;

    username = receive(client_socket); // recv #1 -- the username

    cout << "USER: " << username << " has connected." << endl;

    word = getWord();
    word_len = word.length();

    cout << "The word for user (" << username << "): " << word << endl << endl;

    // create blanks for word
    for(int i = 0; i < word_len; i++)
        progress += "-";

    while(!done){
        guessed = false;
        turnAndProgress = "\nTurn " + to_string(turns) + "\nWord: " +
                          progress + "\n";
        sendString(client_socket, turnAndProgress); // send #1 -- # of turns & blanks
        guess = receive(client_socket); // recv #2 -- user's guess

        guess[0] = toupper(guess[0]);

        for(int i = 0; i < guessedCharPtr; i++){
            if(guess == guessedChars[i]){
                guessed = true;
            }
        }

        // send #2 -- validity if guessed
        if(guessed){
            valid = htons(0);
            sendVal = send(client_socket, &valid, sizeof(uint16_t),0);
            if(sendVal == -1){
                cout << "Error: Send valid failed." << endl;
                close(client_socket);
                pthread_exit(&sendVal);
            }
        }
        else{
            guessedChars[guessedCharPtr] = guess;
            guessedCharPtr++;
            correct = false;
            valid = htons(1);
            sendVal = send(client_socket, &valid, sizeof(uint16_t),0);
            if(sendVal == -1){
                cout << "Error: Send valid failed." << endl;
                close(client_socket);
                pthread_exit(&sendVal);
            }
            for(int i = 0; i < word_len; i++){
                if(word[i] == guess[0]){
                    correct = true;
                    progress[i] = guess[0];
                    correctGuesses++;
                }
            }
            if(correct)
                isCorrect = htons(1);
            else
                isCorrect = htons(0);

            sendVal = send(client_socket, &isCorrect, sizeof(uint16_t),0); // send #3 -- correctness
            if(sendVal == -1){
                cout << "Error: Send valid failed." << endl;
                close(client_socket);
                pthread_exit(&sendVal);
            }

            // send #4 -- if done
            if(correctGuesses == word_len){
                done = true;
                doneSignal = htons(1);
                sendVal = send(client_socket, &doneSignal, sizeof(uint16_t),0); // send #4
                if(sendVal == -1){
                    cout << "Error: Send valid failed." << endl;
                    close(client_socket);
                    pthread_exit(&sendVal);
                }
                response = "\n\nCongratulations! You guessed the word " + word +
                           "!!\nIt took " + to_string(turns) + " guesses to guess the word correctly.\n";

                lbEntries++;

                // calculation of score
                score = (float)turns/(float)word_len;
                cout << username << " has completed the game." << endl;
                cout << username << "'s score: " << score << endl << endl;

                if(lbEntries > 3)
                    lbEntries = 3;

                pthread_mutex_lock(&m);
                
                // assuming that this player has the biggest score in the leader board
                leaderBoard[LB_SIZE - 1].name = username;
                leaderBoard[LB_SIZE - 1].score = score;

                sortLeaderBoard();

                lbRanks = "";

                for(int i = 0; i < lbEntries; i++){
                    lbRanks += "\t" + to_string(i+1) + ". " + leaderBoard[i].name
                      + " " + formatFloat(leaderBoard[i].score) + "\n";
                }
                string final = response + lbHeader + lbRanks;
                sendString(client_socket, final); // send #5 -- congratulatory message + leaderboard
                pthread_mutex_unlock(&m);
            }
            else{
                doneSignal = htons(0);
                sendVal = send(client_socket, &doneSignal, sizeof(uint16_t),0); // send #4
                if(sendVal == -1){
                    cout << "Error: Send valid failed." << endl;
                    close(client_socket);
                    pthread_exit(&sendVal);
                }
            }
        }
        turns++;
    }
    close(client_socket);
    return 0;
}

void sortLeaderBoard()
{
  int min, temp;
  for(int i = 0; i < LB_SIZE - 1; i++){
    min = i;
    for(int j = i + 1; j < LB_SIZE; j++){
      if(leaderBoard[j].score < leaderBoard[min].score)
        min = j;
    }
    swap(leaderBoard[i],leaderBoard[min]);
  }
}

string formatFloat(float f)
{
  ostringstream stream;
  stream.precision(2);
  stream << fixed;
  stream << f;
  return stream.str();
}
