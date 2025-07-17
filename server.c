

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>

#define PORT 8888
#define BUFFER_SIZE 1024
#define MAX_PLAYERS 4
#define DECK_SIZE 52

const char *ranks[] = {"2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A"};
const char *suits[] = {"♠", "♥", "♦", "♣"};

typedef struct {
    int rank; //0–12
    int suit; //0–3
} Card;

Card deck[DECK_SIZE];
Card player_hands[MAX_PLAYERS][2];
Card community_cards[5];
int folded[MAX_PLAYERS];

void shuffle_deck();
void send_to_all(int fds[], int count, const char *msg);
void deal_cards(int player_count);
void card_to_string(Card c, char *str);
int is_flush(Card all[7]);
int is_straight(int rank_counts[13]);
int hand_strength(Card hand[2], Card community[5]);

void ask_players_to_continue(int fds[], int player_count) {
    char buffer[BUFFER_SIZE];
    for (int i = 0; i < player_count; i++) {
        if (folded[i]) continue;
        send(fds[i], " stay in the round or not? (Y/N): ", 42, 0);
        int bytes = recv(fds[i], buffer, BUFFER_SIZE - 1, 0);
        if (bytes > 0) buffer[bytes] = '\0';
        if (buffer[0] == 'N' || buffer[0] == 'n') {
            folded[i] = 1;
            send(fds[i], "folded.\n", 13, 0);
        } else {
            send(fds[i], "stayed in the round.\n", 27, 0);
        }
    }
}

int main() {
    int server_fd, client_fds[MAX_PLAYERS];
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    int player_count;
   
    srand((unsigned int)time(NULL));

    printf("Enter number of players (3 or 4): ");
    scanf("%d", &player_count);
    if (player_count < 3 || player_count > 4) {
        printf("Invalid player count. Exiting.\n");
        return 1;
    }
    void shuffle_deck() {
    int k = 0;
        for (int i = 0; i < 13; i++) {
            for (int j = 0; j < 4; j++) {
                deck[k].rank = i;
                deck[k].suit = j;
                k++;
            }
        }
        for (int i = 0; i < DECK_SIZE; i++) {
            int j = rand() % DECK_SIZE;
            Card tmp = deck[i];
            deck[i] = deck[j];
            deck[j] = tmp;
        }
    }

    void send_to_all(int fds[], int count, const char *msg) {
        for (int i = 0; i < count; i++) {
            send(fds[i], msg, strlen(msg), 0);
        }
    }

    void deal_cards(int player_count) {
        int pos = 0;
        for (int i = 0; i < player_count; i++) {
            player_hands[i][0] = deck[pos++];
            player_hands[i][1] = deck[pos++];
        }
        for (int i = 0; i < 5; i++) {
            community_cards[i] = deck[pos++];
        }
    }

    void card_to_string(Card c, char *str) {
        sprintf(str, "%s%s", ranks[c.rank], suits[c.suit]);
    }

    int is_flush(Card all[7]) {
        int suit_counts[4] = {0};
        for (int i = 0; i < 7; i++) suit_counts[all[i].suit]++;
        for (int i = 0; i < 4; i++) if (suit_counts[i] >= 5) return i;
        return -1;
    }

    int is_straight(int rank_counts[13]) {
        for (int i = 0; i <= 8; i++) {
            int found = 1;
            for (int j = 0; j < 5; j++) {
                if (rank_counts[(i + j) % 13] == 0) found = 0;
            }
            if (found) return (i + 4) % 13;
        }
        // special case A2345
        if (rank_counts[12] && rank_counts[0] && rank_counts[1] && rank_counts[2] && rank_counts[3]) return 3;
        return -1;
    }

    int hand_strength(Card hand[2], Card community[5]) {
        Card all[7];
        memcpy(all, hand, 2 * sizeof(Card));
        memcpy(all + 2, community, 5 * sizeof(Card));

        int rank_counts[13] = {0}, suit_counts[4] = {0};
        for (int i = 0; i < 7; i++) {
            rank_counts[all[i].rank]++;
            suit_counts[all[i].suit]++;
        }

    // check for straight flush
    for (int s = 0; s < 4; s++) {
        if (suit_counts[s] >= 5) {
            int flush_ranks[13] = {0};
            for (int i = 0; i < 7; i++) {
                if (all[i].suit == s) flush_ranks[all[i].rank]++;
            }
            int sf = is_straight(flush_ranks);
            if (sf != -1) return 800 + sf;
        }
    }

    // four of a kind
    for (int i = 12; i >= 0; i--) if (rank_counts[i] == 4) return 700 + i;

    // full house
    int trips = -1, pair = -1;
    for (int i = 12; i >= 0; i--) {
        if (rank_counts[i] >= 3 && trips == -1) trips = i;
        else if (rank_counts[i] >= 2 && pair == -1) pair = i;
    }
    if (trips != -1 && pair != -1) return 600 + trips;

    // flush
    if (is_flush(all) != -1) {
        for (int i = 6; i >= 0; i--) {
            if (all[i].suit == is_flush(all)) return 500 + all[i].rank;
        }
    }

    // straight
    int st = is_straight(rank_counts);
    if (st != -1) return 400 + st;

    // three of a kind
    for (int i = 12; i >= 0; i--) if (rank_counts[i] == 3) return 300 + i;

    // two pair
    int p1 = -1, p2 = -1;
    for (int i = 12; i >= 0; i--) {
        if (rank_counts[i] == 2) {
            if (p1 == -1) p1 = i;
            else if (p2 == -1) return 200 + (p1 > i ? p1 : i);
        }
    }

    // one pair
    for (int i = 12; i >= 0; i--) if (rank_counts[i] == 2) return 100 + i;

    // high card
    for (int i = 12; i >= 0; i--) if (rank_counts[i] > 0) return i;

    return 0;
}
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    listen(server_fd, player_count);

    printf("Waiting for players...\n");
    for (int i = 0; i < player_count; i++) {
        client_fds[i] = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        printf("Player %d connected.\n", i + 1);
        sprintf(buffer, "Welcome Player %d! Waiting for others...\n", i + 1);
        send(client_fds[i], buffer, strlen(buffer), 0);
        folded[i] = 0;
    }

    send_to_all(client_fds, player_count, "All players connected! Starting game...\n\n");

    shuffle_deck();
    deal_cards(player_count);

    for (int i = 0; i < player_count; i++) {
        char hand_msg[BUFFER_SIZE];
        char c1[8], c2[8];
        card_to_string(player_hands[i][0], c1);
        card_to_string(player_hands[i][1], c2);
        sprintf(hand_msg, "Your hand: %s %s\n", c1, c2);
        send(client_fds[i], hand_msg, strlen(hand_msg), 0);
    }

    //翻牌
    send_to_all(client_fds, player_count, "\nFlop:\n");
    for (int i = 0; i < 3; i++) {
        char card_str[8];
        card_to_string(community_cards[i], card_str);
        sprintf(buffer, "%s ", card_str);
        send_to_all(client_fds, player_count, buffer);
    }
    send_to_all(client_fds, player_count, "\n\n");
    ask_players_to_continue(client_fds, player_count);

    //轉牌
    send_to_all(client_fds, player_count, "\nTurn:\n");
    card_to_string(community_cards[3], buffer);
    strcat(buffer, "\n\n");
    send_to_all(client_fds, player_count, buffer);
    ask_players_to_continue(client_fds, player_count);

    //河牌
    send_to_all(client_fds, player_count, "\nRiver:\n");
    card_to_string(community_cards[4], buffer);
    strcat(buffer, "\n\n");
    send_to_all(client_fds, player_count, buffer);
    ask_players_to_continue(client_fds, player_count);

    // 比牌型大小
    int best = -1, winner = -1;
    for (int i = 0; i < player_count; i++) {
        if (folded[i]) continue;
        int score = hand_strength(player_hands[i], community_cards);
        if (score > best) {
            best = score;
            winner = i;
        }
    }

    if (winner != -1) {
        sprintf(buffer, "\nWinner is Player %d!\n", winner + 1);
    } else {
        sprintf(buffer, "\nNo winner. All players folded.\n");
    }
    send_to_all(client_fds, player_count, buffer);

    for (int i = 0; i < player_count; i++) {
        send(client_fds[i], "Game over.\n", 11, 0);
        close(client_fds[i]);
    }
    close(server_fd);
    return 0;
}
