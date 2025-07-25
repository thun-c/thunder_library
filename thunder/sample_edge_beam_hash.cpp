/**************************************************************/
// Euler Tour の辺を保持する差分更新ビームサーチライブラリを使うサンプルコード
// 全体行数が長そうに見えるが、ライブラリ部分を除くとそれなりに短い。
// ライブラリ部分はnamespaceで囲っているので、折りたたんで読むことを推奨。
/**************************************************************/

#include "lib/edge_beam.cpp"
using namespace std;

/// @brief TODO: Hashを表す型を非負整数型で指定(e.g. uint32_t, uint64_t)
using Hash = uint32_t;

/// @brief TODO: 状態遷移を行うために必要な情報
/// @note メモリ使用量をできるだけ小さくしてください
struct Action
{
    // TODO: 何書いてもいい
};

/// @brief TODO: コストを表す型を算術型で指定(e.g. int, long long, double)
using Cost = int;

/// @brief TODO: 深さ優先探索に沿って更新する情報をまとめたクラス
/// @note expand, move_forward, move_backward の3つのメソッドを実装する必要がある
/// @note template<typename Selector>を最初に記述する必要がある
template <typename Selector>
class StateBase
{

public:
    /// @brief TODO: 次の状態候補を全てselectorに追加する
    /// @param parent 今のノードID（次のノードにとって親となる）
    /// @param multi_selectors 次の状態候補を追加するためのselector
    void expand(int parent, Selector &selector)
    {
        // 合法手の数だけループ
        {
            Action new_action; // 新しいactionを作成
            Cost new_cost;     // 新しいコストを作成

            // move_forward(new_action); // 自由だが、ここでmove_forwardすると楽
            Hash new_hash; // move_forward内か、その後にthisから計算すると楽
            Cost new_cost; // move_forward内か、その後にthisから計算すると楽
            bool finished; // ターン最小化問題で問題を解き終わったか
            // move_backward(new_action);// 自由だが、ここでmove_forwardすると楽

            selector.push(new_action, new_cost, new_hash, parent, finished);
        }
    }

    /// @brief TODO: actionを実行して次の状態に遷移する
    void move_forward(const Action action)
    {
    }

    /// @brief TODO: actionを実行する前の状態に遷移する
    /// @param action 実行したaction
    void move_backward(const Action action)
    {
    }

    /// @brief TODO: 初期状態のコストとハッシュを返す
    /// @return 初期状態のコストとハッシュ
    pair<Cost, Hash> make_initial_node()
    {
        return {0, 0};
    }
};

// TODO: Hash,Action,Cost,StateBase の定義より後に以下を記述
using BeamSearchUser = EdgeBeamSearch<Hash, Action, Cost, StateBase>;
BeamSearchUser beam_search;
using State = StateBase<BeamSearchUser::Selector>;
// TODO: ここまで

int main()
{
    // 適切な設定を問題ごとに指定
    BeamSearchUser::Config config = {
        .max_turn = 0,
        .beam_width = 0,
        .tour_capacity = 0,
        .hash_map_capacity = 0,              // 要素数の16倍ぐらいは必要らしい
        .return_finished_immediately = true, // ターン最小化問題で実行可能解を見つけたらすぐに終了するか
    };
    State state;
    auto output = beam_search.beam_search(config, state);
    // outputを問題設定に従い標準出力に掃き出す
    return 0;
}
