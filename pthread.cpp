#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <string>
#include <cstdlib>
#include <iostream>
#include <deque>
#include <algorithm>
#include <fstream>
#include <stdio.h>


#define TIMES_SHUFFLE 4
#define NUM_THREADS 3
#define NUM_ROUNDS 3

#define NO_WINNER 1
#define HAS_WINNER 0



using namespace std;

struct Deck
{
   deque<int> cards;
};

struct Player
{
   int id;
   deque<int> hand;
};

pthread_mutex_t Mdeck;
pthread_mutex_t Mstatus;
pthread_cond_t status_cv;
Deck* deck = new Deck;
int round_status = NO_WINNER;

void create_deck(Deck*&);
void shuffle_deck(Deck*&, const int = 1);
void deal(Player* player[], const int);
void print_game_status(const Player* const player[], const int);
void print_deck(const Deck*, const int P_option = 0);
void print_hand(const Player* p, const int P_option = 0);
void draw(Player*& p, Deck*& d);
void discard_card(Player*&, Deck*&);
void push_back_deck(Deck*&, const int);
int pop_front_deck();
void* make_move(void*);
void exit_round(Player*& p);
void log_event(const string);

int main(int argc, char *argv[]) {

 
   if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <seed_int> " << std::endl;
        return 1;
   }
   srand(atoi(argv[1]));

   Player* p1 = new Player;
   Player* p2 = new Player;
   Player* p3 = new Player;
   Player* player[NUM_THREADS] = {p1,p2,p3};

   for (int n = 0; n < NUM_THREADS; n++) {
      player[n]->id = n + 1; 
   }

   pthread_t threads[NUM_THREADS];
   pthread_mutex_init(&Mdeck, NULL);
   pthread_mutex_init(&Mstatus, NULL);

   for (int r = 0; r < NUM_ROUNDS; r++) {
      create_deck(deck);
      cout << "*** ROUND " << (r + 1) << endl;
      deal(player, NUM_THREADS);
      print_game_status(player, NUM_THREADS);

      int rc;
      while (round_status) {
         for (int i = 0; i < NUM_THREADS; i++) {
            rc = pthread_create(&threads[i], NULL, make_move, (void *)player[i]);
            if (rc) {
               printf("[-]ERROR; return code from pthread_create() is %d\n", rc);
               exit(-1);
            }
         }
         for (int i = 0; i < NUM_THREADS; i++) {
            if (pthread_join(threads[i], NULL)) {
               fprintf(stderr, "[-]Error joining thread\n");
               exit(2);
            }
         }
      }
      print_game_status(player, NUM_THREADS);

      for (int n = 0; n < NUM_THREADS; n++) {
         string e = "PLAYER ";
         e += to_string(player[n]->id);
         e += ": exits round";
         exit_round(player[n]);
         //log_event(e);
      }
      round_status = NO_WINNER;
   }
   pthread_mutex_destroy(&Mdeck);
   pthread_mutex_destroy(&Mstatus);
   pthread_exit(NULL);
}

void create_deck(Deck*& d) {
   delete d;
   d = new Deck;
   for (int i = 1; i <= 13; i++) {
      for (int j = 0; j < 4; j++) {
         d->cards.push_back(i);
      }
   }
   shuffle_deck(d,TIMES_SHUFFLE);
}

void shuffle_deck(Deck*& d, const int times) {
   int sz = d->cards.size();
   int position;
   //log_event("DEALER: shuffling");
   for (int j = 0; j < times; j++) {
      for (int i = 0 ; i < sz; i++) {
         position = rand() % sz;
#ifdef DEBUG_DECK
         cout << "[+]swapping positions " << i << " & " << position << endl;
#endif
         iter_swap(d->cards.begin() + i, d->cards.begin() + position);
      }
   }
}

void deal(Player* player[], const int num_players) {
   //log_event("DEALER: dealing round");
   for (int i = 0; i < num_players; i++) {
      player[i]->hand.push_back(pop_front_deck());
   }
}


void print_game_status(const Player* const player[], const int num_players) {
   cout << "                     " << endl;
   for (int i = 0; i < num_players; i++) {
      print_hand(player[i], 1);
   }
   print_deck(deck,1);
   cout << "                      " << endl;
}

void print_deck(const Deck* d, const int P_option) {
   string e = "DECK: ";

   for (int i = d->cards.size() - 1; i >= 0; i--) {
      e +=  to_string(d->cards.at(i)); e+= " ";
   }
   log_event(e);

   if (P_option) {
      cout << e << endl;
   }
}

void print_hand(const Player* p, const int P_option) {
   int hand[2];
   string e = "PLAYER: "; 
   e += to_string(p->id); 
   e += "\nHAND: ";

   for (int i = p->hand.size() - 1; i >= 0; i--) {
      e +=  to_string(p->hand.at(i)); 
      e += " ";
   }
   log_event(e);
   if (P_option) {
      cout << e << " " << endl;
      for(int i = p->hand.size() - 1; i >= 0; i--){
        hand[i] = p->hand.at(i);
      }
      if(p->hand.size() == 1){
        cout << "WIN: no" << endl;
      }
      else
          if(hand[0] == hand[1])
            cout << "WIN: yes" << endl;
   }
}


void draw(Player*& p, Deck*& d) {
   string e = "";
   e += "PLAYER "; e += to_string(p->id); e+= ": draws ";
   if (d->cards.size() > 0) {
      int card = pop_front_deck();
      e += to_string(card);
      p->hand.push_back(card);
   } else {
      e += "nothing from empty deck." ;
   }
   log_event(e);
}

void discard_card(Player*& player, Deck*& d) {
   string e = "";
   e += "PLAYER "; 
   e += to_string(player->id); 
   e += ": discards ";

   int cards_in_hand = player->hand.size();
   int rand_picked_card = rand() % cards_in_hand;

   e += to_string(player->hand.at(rand_picked_card));

   push_back_deck(d, player->hand.at(rand_picked_card));
   player->hand.erase(player->hand.begin() + rand_picked_card);
   log_event(e);
}

void push_back_deck(Deck*& d, const int card) {
   d->cards.push_back(card);
}


int pop_front_deck() {
   int card =  deck->cards.front();
   deck->cards.pop_front();
   return card;
}


void* make_move(void* player) {
   Player* p = (Player*)player;
   pthread_mutex_lock(&Mdeck);
   if (p->hand.size() >= 2) {
      discard_card(p, deck);
      print_hand(p);  
   }
#ifdef FAKEOUT_DELAY
   usleep(rand() % 1000000);
#endif
   draw(p, deck);
   print_hand(p);
   pthread_mutex_lock (&Mstatus);
   if (p->hand.at(0) ==  p->hand.at(1)) {
      pthread_cond_signal(&status_cv);
      round_status = HAS_WINNER;
      string e = "PLAYER ";
      e += to_string(p->id); e+= " WINS!";
      //log_event(e);
   }
   else
       discard_card(p, deck);
   
   pthread_mutex_unlock (&Mstatus);
   pthread_mutex_unlock (&Mdeck);

   pthread_exit(NULL);
}


void exit_round(Player*& p) {
   int cards_in_hand = p->hand.size();
   for (int i = cards_in_hand - 1; i >= 0; i--) {
      p->hand.erase(p->hand.begin() + i);
   }
}

void log_event(string e) {
#ifdef COUT_EVENTS
   cout << e << endl;
#else
   ofstream fout;
   ifstream fin;
   fin.open("game.log");
   fout.open ("game.log",ios::app); 
   if (fin.is_open()) {
      fout << e << endl;
   }
   fin.close();
   fout.close();
 #endif
}