#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <map>
#include <string>

using namespace std;

random_device rd;
mt19937 gen(rd());
uniform_int_distribution<int> dist(1, 6);

struct ScoreCard {
    map<string, int> scores;
    bool used[12] = { false };
};

void printDice(const vector<int>& dice) {
    cout << "주사위: ";
    for (int i : dice) cout << i << " ";
    cout << endl;
}

vector<int> rollDice(const vector<bool>& hold) {
    vector<int> dice(5);
    for (int i = 0; i < 5; i++) {
        if (!hold[i]) dice[i] = dist(gen);
    }
    return dice;
}

int sumDice(const vector<int>& dice) {
    int s = 0;
    for (int d : dice) s += d;
    return s;
}

int countNum(const vector<int>& dice, int num) {
    return count(dice.begin(), dice.end(), num);
}

// 족보 점수 계산
int calcScore(const string& category, const vector<int>& dice) {
    vector<int> d = dice;
    sort(d.begin(), d.end());
    int sum = sumDice(d);
    if (category == "Aces") return countNum(d, 1) * 1;
    if (category == "Deuces") return countNum(d, 2) * 2;
    if (category == "Threes") return countNum(d, 3) * 3;
    if (category == "Fours") return countNum(d, 4) * 4;
    if (category == "Fives") return countNum(d, 5) * 5;
    if (category == "Sixes") return countNum(d, 6) * 6;
    if (category == "Choice") return sum;

    // Four of a Kind
    for (int i = 1; i <= 6; i++) if (countNum(d, i) >= 4) return sum;

    // Full House (3 + 2)
    bool three = false, two = false;
    for (int i = 1; i <= 6; i++) {
        int c = countNum(d, i);
        if (c == 3) three = true;
        if (c == 2) two = true;
    }
    if (category == "Full House" && three && two) return 25;

    // Small Straight (연속 4)
    if (category == "S. Straight") {
        vector<vector<int>> smalls = { {1,2,3,4}, {2,3,4,5}, {3,4,5,6} };
        for (auto& s : smalls)
            if (includes(d.begin(), d.end(), s.begin(), s.end())) return 30;
    }

    // Large Straight (연속 5)
    if (category == "L. Straight") {
        vector<vector<int>> larges = { {1,2,3,4,5}, {2,3,4,5,6} };
        for (auto& s : larges)
            if (includes(d.begin(), d.end(), s.begin(), s.end())) return 40;
    }

    // Yacht (5개 전부 같을 때)
    if (category == "Yacht") {
        if (countNum(d, d[0]) == 5) return 50;
    }

    return 0;
}

void printCategories(const ScoreCard& card) {
    vector<string> categories = {
        "Aces", "Deuces", "Threes", "Fours", "Fives", "Sixes",
        "Choice", "4 of a Kind", "Full House", "S. Straight", "L. Straight", "Yacht"
    };
    cout << "\n=== 점수판 ===\n";
    for (int i = 0; i < categories.size(); i++) {
        cout << i + 1 << ". " << categories[i] << " : ";
        if (card.used[i]) cout << card.scores.at(categories[i]) << "점";
        cout << endl;
    }
}

int main() {
    vector<string> categories = {
        "Aces", "Deuces", "Threes", "Fours", "Fives", "Sixes",
        "Choice", "4 of a Kind", "Full House", "S. Straight", "L. Straight", "Yacht"
    };

    ScoreCard card;
    for (auto& c : categories) card.scores[c] = 0;

    for (int turn = 1; turn <= 12; turn++) {
        cout << "\n==== 턴 " << turn << " ====\n";

        vector<int> dice(5);
        vector<bool> hold(5, false);

        // 첫 굴리기
        for (int i = 0; i < 5; i++) dice[i] = dist(gen);
        printDice(dice);

        for (int r = 0; r < 2; r++) {
            cout << "고정할 주사위 번호 입력 (1~5, 띄어쓰기, 0=그만): ";
            string line;
            getline(cin, line);
            if (line == "0" || line.empty()) break;
            fill(hold.begin(), hold.end(), false);
            for (char c : line) if (isdigit(c)) hold[c - '1'] = true;
            for (int i = 0; i < 5; i++) if (!hold[i]) dice[i] = dist(gen);
            printDice(dice);
        }

        printCategories(card);
        int choice;
        cout << "기록할 족보 번호 선택: ";
        cin >> choice;
        cin.ignore();

        while (card.used[choice - 1]) {
            cout << "이미 사용된 항목입니다. 다시 선택: ";
            cin >> choice;
            cin.ignore();
        }

        string category = categories[choice - 1];
        int score = calcScore(category, dice);
        card.scores[category] = score;
        card.used[choice - 1] = true;
        cout << category << "에 " << score << "점을 기록했습니다.\n";
    }

    int total = 0;
    for (auto& s : card.scores) total += s.second;
    cout << "\n게임 종료! 총점: " << total << "점\n";
}
