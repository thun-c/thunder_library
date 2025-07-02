/**************************************************************/
// ターンをスキップできる差分更新ビームサーチライブラリを使うサンプルコード(hashなし版)
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

constexpr int max_turn = 10000;
constexpr size_t beam_width = 1700;
constexpr size_t nodes_capacity = 25 * beam_width;
constexpr uint32_t hash_map_capacity = 16 * beam_width;

#include "lib/timer.cpp"
#include "lib/skip_beam.cpp"
using namespace std;

constexpr int n = 30;
constexpr int m = n * (n + 1) / 2;

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

inline int get_pyramid_index(int x, int y)
{
    return x * (x - 1) / 2 + y;
}
constexpr int target_coefficient = 600;

/// @brief TODO: 状態遷移を行うために必要な情報
/// @note メモリ使用量をできるだけ小さくしてください
struct Action
{
    using ll = long long;

    ll xyxyxy;

    Action(int x1, int y1, int x2, int y2, ll x3, ll y3)
    {
        xyxyxy = x1 | (y1 << 8) | (x2 << 16) | (y2 << 24) | (x3 << 32) | (y3 << 48);
    }

    tuple<int, int, int, int, int, int> decode() const
    {
        return {xyxyxy & 255, (xyxyxy >> 8) & 255, (xyxyxy >> 16) & 255, (xyxyxy >> 24) & 255, (xyxyxy >> 32) & 255, xyxyxy >> 48};
    }
};

/// @brief TODO: コストを表す型を算術型で指定(e.g. int, long long, double)
using Cost = int;

/// @brief TODO: 深さ優先探索に沿って更新する情報をまとめたクラス
/// @note expand, move_forward, move_backward の3つのメソッドを実装する必要がある
/// @note template<typename MultiSelectors>を最初に記述する必要がある
template <typename MultiSelectors>
class StateBase
{

private:
    int target_ball_;
    int potential_;
    vector<vector<int>> b_;
    array<pair<int, int>, m> positions_;
    vector<int> target_ball_history_;

public:
    /// @brief TODO: 次の状態候補を全てselectorに追加する
    /// @param parent 今のノードID（次のノードにとって親となる）
    /// @param multi_selectors 次の状態候補を追加するためのselector
    void expand(int parent, MultiSelectors &multi_selectors)
    {

        auto push_candidate2 = [&](int x1, int y1, int x2, int y2)
        {
            assert(x1 > x2);
            assert(b_[x1][y1] < b_[x2][y2]);

            Action new_action(x1, y1, x2, y2, 0, 0);

            move_forward(new_action);
            auto new_target_ball = this->target_ball_;
            auto new_potential = this->potential_;
            auto new_cost = evaluate();
            move_backward(new_action);

            bool finished = (new_target_ball == m);

            multi_selectors.push(new_action, new_cost, parent, finished, 1);
        };

        auto push_candidate3 = [&](int x1, int y1, int x2, int y2, int x3, int y3)
        {
            Action new_action(x1, y1, x2, y2, x3, y3);

            move_forward(new_action);
            auto new_target_ball = this->target_ball_;
            auto new_potential = this->potential_;
            auto new_cost = evaluate();
            move_backward(new_action);
            bool finished = (new_target_ball == m);

            multi_selectors.push(new_action, new_cost, parent, finished, 2);
        };

        auto [x, y] = positions_[target_ball_];

        if (can_move_left(x, y))
        {
            push_candidate2(x, y, x - 1, y - 1);
            if (can_move_left(x - 1, y - 1))
            {
                push_candidate3(x - 1, y - 1, x - 2, y - 2, x, y);
            }
            if (can_move_right(x - 1, y - 1))
            {
                push_candidate3(x - 1, y - 1, x - 2, y - 1, x, y);
            }
        }
        if (can_move_right(x, y))
        {
            push_candidate2(x, y, x - 1, y);
            if (can_move_left(x - 1, y))
            {
                push_candidate3(x - 1, y, x - 2, y - 1, x, y);
            }
            if (can_move_right(x - 1, y))
            {
                push_candidate3(x - 1, y, x - 2, y, x, y);
            }
        }
    }

    /// @brief TODO: actionを実行して次の状態に遷移する
    void move_forward(const Action action)
    {
        target_ball_history_.emplace_back(target_ball_);

        auto [x1, y1, x2, y2, x3, y3] = action.decode();
        potential_ += b_[x1][y1] - b_[x2][y2];
        swap_balls(x1, y1, x2, y2);
        if (x3)
        {
            potential_ += b_[x3][y3] - b_[x2][y2];
            swap_balls(x1, y1, x3, y3);
        }
        target_ball_ = update_target_ball(target_ball_);
    }

    /// @brief TODO: actionを実行する前の状態に遷移する
    /// @param action 実行したaction
    void move_backward(const Action action)
    {
        auto [x1, y1, x2, y2, x3, y3] = action.decode();
        if (x3)
        {
            swap_balls(x1, y1, x3, y3);
            potential_ -= b_[x3][y3] - b_[x2][y2];
        }
        swap_balls(x1, y1, x2, y2);
        potential_ -= b_[x1][y1] - b_[x2][y2];
        if (!target_ball_history_.empty())
        {
            target_ball_ = target_ball_history_.back();
            target_ball_history_.pop_back();
        }
    }

    Cost evaluate() const
    {
        return potential_ - target_coefficient * target_ball_;
    }

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

// TODO: Action,Cost,StateBase の定義より後に以下を記述
using BeamSearchUser = BeamSearchNoHash<Action, Cost, StateBase>;
BeamSearchUser beam_search;
using State = StateBase<BeamSearchUser::MultiSelectors>;
// TODO: ここまで

struct Solver
{
    const Input input;
    vector<Action> output;

    Solver(const Input &input) : input(input) {}

    void solve()
    {
        BeamSearchUser::Config config = {
            max_turn,
            beam_width,
            nodes_capacity};
        State state(input);
        BeamSearchUser::Node root(Action(0, 0, 0, 0, 0, 0), state.evaluate());
        output = beam_search.beam_search(config, state, root);
    }

    void print() const
    {
        int output_size = output.size();
        for (Action action : output)
        {
            auto [_x1, _y1, _x2, _y2, x3, y3] = action.decode();
            if (x3)
            {
                ++output_size;
            }
        }
        cout << output_size << "\n";
        for (Action action : output)
        {
            auto [x1, y1, x2, y2, x3, y3] = action.decode();
            cout << x1 << " " << y1 << " " << x2 << " " << y2 << "\n";
            if (x3)
            {
                cout << x1 << " " << y1 << " " << x3 << " " << y3 << "\n";
            }
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
    cerr << "end in " << timer.getTime() << " sec" << endl;

    return 0;
}
