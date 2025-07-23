/**************************************************************/
// Euler Tour の辺を保持する差分更新ビームサーチライブラリを使うサンプルコード(hashなし版)
// 全体行数が長そうに見えるが、ライブラリ部分を除くとそれなりに短い。
// ライブラリ部分はnamespaceで囲っているので、折りたたんで読むことを推奨。
/**************************************************************/

#pragma GCC optimize("O3")
#pragma GCC optimize("unroll-loops")
#include <bits/stdc++.h>
#ifdef ONLINE_JUDGE
// clang-format off
#define cerr if (false) std::cerr
// clang-format on
#define NDEBUG
#endif
#include <atcoder/segtree>
using namespace std;

constexpr int n = 30;
constexpr int m = n * (n + 1) / 2;

constexpr int max_turn = 10000;
constexpr size_t beam_width = 1700;
constexpr size_t tour_capacity = 15 * beam_width;
constexpr uint32_t hash_map_capacity = 16 * 3 * beam_width;
constexpr int target_coefficient = 600;
inline int get_pyramid_index(int x, int y)
{
    return x * (x - 1) / 2 + y;
}

#include "lib/timer.cpp"
#include "lib/edge_beam.cpp"
struct Input
{
    vector<vector<int>> b;

    void input()
    {
        b.resize(n);
        for (int x = 0; x < n; ++x)
        {
            b[x] = vector<int>(x + 1);
            for (int y = 0; y <= x; ++y)
            {
                cin >> b[x][y];
            }
        }
    }
};
using Cost = int;

// 状態遷移を行うために必要な情報
// メモリ使用量をできるだけ小さくしてください
struct Action
{
    int xyxy;

    Action(int x1, int y1, int x2, int y2)
    {
        xyxy = x1 | (y1 << 8) | (x2 << 16) | (y2 << 24);
    }

    tuple<int, int, int, int> decode() const
    {
        return {xyxy & 255, (xyxy >> 8) & 255, (xyxy >> 16) & 255, xyxy >> 24};
    }

    bool operator==(const Action &other) const
    {
        return xyxy == other.xyxy;
    }
};

// 深さ優先探索に沿って更新する情報をまとめたクラス

template <typename Selector>
class StateBase
{
public:
    vector<vector<int>> b_;
    array<pair<int, int>, m> positions_;
    int target_ball_;
    int potential_;
    vector<int> target_ball_history_;

    StateBase() = default;

    explicit StateBase(const Input &input)
    {
        target_ball_history_.reserve(max_turn);
        b_ = input.b;

        for (int x = 0; x < n; ++x)
        {
            for (int y = 0; y <= x; ++y)
            {
                positions_[b_[x][y]] = {x, y};
            }
        }

        auto new_target_ball = update_target_ball(0);
        this->potential_ = 0;
        this->target_ball_ = new_target_ball;
    }

    // Costの初期値を返す
    Cost make_initial_node()
    {
        return 0;
    }

    Cost evaluate() const
    {
        return potential_ - target_coefficient * target_ball_;
    }

    // 次の状態候補を全てselectorに追加する
    // 引数
    //   evaluator : 今の評価器
    //   parent    : 今のノードID（次のノードにとって親となる）
    void expand(int parent, Selector &selector)
    {
        auto push_candidate = [&](int x1, int y1, int x2, int y2)
        {
            assert(x1 > x2);
            assert(b_[x1][y1] < b_[x2][y2]);

            Action new_action(x1, y1, x2, y2);

            move_forward(new_action);
            auto new_target_ball = this->target_ball_;
            auto new_potential = this->potential_;
            auto new_cost = evaluate();
            move_backward(new_action);

            bool finished = (new_target_ball == m);

            selector.push(new_action, new_cost, parent, finished);
        };

        auto [x, y] = positions_[target_ball_];

        if (can_move_left(x, y))
        {
            push_candidate(x, y, x - 1, y - 1);
            if (can_move_left(x - 1, y - 1))
            {
                push_candidate(x - 1, y - 1, x - 2, y - 2);
            }
            if (can_move_right(x - 1, y - 1))
            {
                push_candidate(x - 1, y - 1, x - 2, y - 1);
            }
        }
        if (can_move_right(x, y))
        {
            push_candidate(x, y, x - 1, y);
            if (can_move_left(x - 1, y))
            {
                push_candidate(x - 1, y, x - 2, y - 1);
            }
            if (can_move_right(x - 1, y))
            {
                push_candidate(x - 1, y, x - 2, y);
            }
        }
    }

    // actionを実行して次の状態に遷移する
    void move_forward(Action action)
    {
        target_ball_history_.emplace_back(target_ball_);

        auto [x1, y1, x2, y2] = action.decode();
        potential_ += b_[x1][y1] - b_[x2][y2];
        swap_balls(x1, y1, x2, y2);
        target_ball_ = update_target_ball(target_ball_);
    }

    // actionを実行する前の状態に遷移する
    // 今の状態は、親からactionを実行して遷移した状態である
    void move_backward(Action action)
    {
        auto [x1, y1, x2, y2] = action.decode();
        swap_balls(x1, y1, x2, y2);
        potential_ -= b_[x1][y1] - b_[x2][y2];
        if (!target_ball_history_.empty())
        {
            target_ball_ = target_ball_history_.back();
            target_ball_history_.pop_back();
        }
    }

    void swap_balls(int x1, int y1, int x2, int y2)
    {
        int b1 = b_[x1][y1];
        int b2 = b_[x2][y2];
        b_[x1][y1] = b2;
        b_[x2][y2] = b1;
        positions_[b2] = {x1, y1};
        positions_[b1] = {x2, y2};
    }

    bool can_move_left(int x, int y) const
    {
        return y && b_[x - 1][y - 1] > b_[x][y];
    }

    bool can_move_right(int x, int y) const
    {
        return y < x && b_[x - 1][y] > b_[x][y];
    }

    int update_target_ball(int target_ball) const
    {
        while (target_ball < m)
        {
            auto [x, y] = positions_[target_ball];
            if (can_move_left(x, y) || can_move_right(x, y))
            {
                break;
            }
            else
            {
                ++target_ball;
            }
        }
        return target_ball;
    }
};
using BeamSearchUser = EdgeBeamSearchNoHash<Action, Cost, StateBase>;
using State = StateBase<BeamSearchUser::Selector>;
BeamSearchUser beam_search;
struct Solver
{
    const Input input;
    vector<Action> output;

    Solver(const Input &input) : input(input) {}

    void solve()
    {
        BeamSearchUser::Config config = {
            .max_turn = max_turn,
            .beam_width = beam_width,
            .tour_capacity = tour_capacity,
            .return_finished_immediately = true};
        State state(input);
        output = beam_search.beam_search(config, state);
    }

    void print() const
    {
        cout << output.size() << "\n";
        for (Action action : output)
        {
            auto [x1, y1, x2, y2] = action.decode();
            cout << x1 << " " << y1 << " " << x2 << " " << y2 << "\n";
        }
    }
};

int main()
{
    ios::sync_with_stdio(false);
    std::cin.tie(nullptr);
    Input input;
    input.input();

    Solver solver(input);
    solver.solve();
    solver.print();
    cerr << timer_library::timer.getTime() << " sec" << endl;

    return 0;
}
