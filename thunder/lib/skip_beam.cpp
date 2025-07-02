/**************************************************************/
// ターンをスキップできる差分更新ビームサーチライブラリ
// Hashを用いた同一盤面除去をする場合は
// Hash, Action, Cost, State を自分で定義、実装して
// using BeamSearchUser = BeamSearch<Hash, Action, Cost, StateBase>;
// Hashの処理を記述するのが面倒な場合は
// Action, Cost, State を自分で定義、実装して
// using BeamSearchUser = BeamSearchNoHash<Action, Cost, StateBase>;
// と記述し、
// BeamSearchUser beam_search;
// を用いてビームサーチを実行する。
// Action以外はそれぞれのconceptに準拠している必要がある。
// テンプレートメタプログラミングによって実現しているので、
// 抽象クラスを継承する場合と比べてオーバーヘッドがかからないはず。
// eijirouさんの記事を参考にしているが、
// Evaluatorを使っていないのでどこを変えるべきかがやや明確な気がする。
// 参考: https://eijirou-kyopro.hatenablog.com/entry/2024/02/01/115639
// 要件
// ac-liblrary: https://github.com/atcoder/ac-library
// 推奨
// 単にコピペしただけでも使えるが、ymatsuxさんの分割コンパイルツールと合わせると
// ローカルのコードがすっきりしてオススメ
// https://github.com/ymatsux/competitive-programming/tree/main/combiner
/**************************************************************/
#pragma once
#ifndef SKIP_BEAM_HPP
#define SKIP_BEAM_HPP
#include <bits/stdc++.h>
#include <atcoder/segtree>
// 内部のusing namespace std;が他のプログラムを破壊する可能性があるため、
// ライブラリ全体をnamespaceで囲っている。
namespace skip_beam_library
{
    using namespace std;

    // メモリの再利用を行いつつ集合を管理するクラス
    template <class T>
    class ObjectPool
    {
    public:
        // 配列と同じようにアクセスできる
        T &operator[](int i)
        {
            return data_[i];
        }

        // 配列の長さを変更せずにメモリを確保する
        void reserve(size_t capacity)
        {
            data_.reserve(capacity);
        }

        // 要素を追加し、追加されたインデックスを返す
        int push(const T &x)
        {
            if (garbage_.empty())
            {
                data_.push_back(x);
                return data_.size() - 1;
            }
            else
            {
                int i = garbage_.top();
                garbage_.pop();
                data_[i] = x;
                return i;
            }
        }

        // 要素を（見かけ上）削除する
        void pop(int i)
        {
            garbage_.push(i);
        }

        // 使用した最大のインデックス(+1)を得る
        // この値より少し大きい値をreserveすることでメモリの再割り当てがなくなる
        size_t size()
        {
            return data_.size();
        }

    private:
        vector<T> data_;
        stack<int> garbage_;
    };

    // 連想配列
    // Keyにハッシュ関数を適用しない
    // open addressing with linear probing
    // unordered_mapよりも速い
    // nは格納する要素数よりも4~16倍ほど大きくする
    template <class Key, class T>
    struct HashMap
    {
    public:
        explicit HashMap(uint32_t n)
        {
            n_ = n;
            valid_.resize(n_, false);
            data_.resize(n_);
        }

        // 戻り値
        // - 存在するならtrue、存在しないならfalse
        // - index
        pair<bool, int> get_index(Key key) const
        {
            Key i = key % n_;
            while (valid_[i])
            {
                if (data_[i].first == key)
                {
                    return {true, i};
                }
                if (++i == n_)
                {
                    i = 0;
                }
            }
            return {false, i};
        }

        // 指定したindexにkeyとvalueを格納する
        void set(int i, Key key, T value)
        {
            valid_[i] = true;
            data_[i] = {key, value};
        }

        // 指定したindexのvalueを返す
        T get(int i) const
        {
            assert(valid_[i]);
            return data_[i].second;
        }

        void clear()
        {
            fill(valid_.begin(), valid_.end(), false);
        }

    private:
        uint32_t n_;
        vector<bool> valid_;
        vector<pair<Key, T>> data_;
    };

    template <typename HashType>
    concept HashConcept = requires(HashType hash) {
        { std::is_unsigned_v<HashType> };
    };
    template <typename CostType>
    concept CostConcept = requires(CostType cost) {
        { std::is_arithmetic_v<CostType> };
    };

    template <typename StateType, typename HashType, typename CostType, typename ActionType, typename SelectorType>
    concept StateConcept = HashConcept<HashType> &&
                           CostConcept<CostType> &&
                           requires(StateType state, SelectorType selector) {
                               { state.expand(std::declval<int>(), selector) } -> same_as<void>;
                               { state.move_forward(std::declval<ActionType>()) } -> same_as<void>;
                               { state.move_backward(std::declval<ActionType>()) } -> same_as<void>;
                           };

    template <HashConcept Hash, typename Action, CostConcept Cost, template <typename> class State>
    struct BeamSearch
    {
        // 展開するノードの候補を表す構造体
        struct Candidate
        {
            Action action;
            Hash hash;
            int parent;
            Cost cost;

            Candidate(Action action, Hash hash, int parent, Cost cost) : action(action),
                                                                         hash(hash),
                                                                         parent(parent),
                                                                         cost(cost) {}
        };

        // ビームサーチの設定
        struct Config
        {
            int max_turn;
            size_t beam_width;
            size_t nodes_capacity;
            uint32_t hash_map_capacity;
        };

        static pair<Cost, int> max_func(pair<Cost, int> a, pair<Cost, int> b)
        {
            if (a.first >= b.first)
            {
                return a;
            }
            else
            {
                return b;
            }
        };
        static pair<Cost, int> min_func()
        {
            return make_pair(numeric_limits<Cost>::min(), -1);
        };

        // 削除可能な優先度付きキュー
        using MaxSegtree = atcoder::segtree<
            pair<Cost, int>,
            max_func,
            min_func>;

        // ノードの候補から実際に追加するものを選ぶクラス
        // ビーム幅の個数だけ、評価がよいものを選ぶ
        // ハッシュ値が一致したものについては、評価がよいほうのみを残す
        class Selector
        {
        public:
            explicit Selector(const Config &config) : hash_to_index_(config.hash_map_capacity)
            {
                beam_width = config.beam_width;
                candidates_.reserve(beam_width);
                full_ = false;
                st_original_.resize(beam_width);
            }

            // 候補を追加する
            // ターン数最小化型の問題で、candidateによって実行可能解が得られる場合にのみ finished = true とする
            // ビーム幅分の候補をCandidateを追加したときにsegment treeを構築する
            bool push(const Action &action, const Cost cost, const Hash hash, const int parent, bool finished)
            {
                if (finished)
                {
                    finished_candidates_.emplace_back(Candidate(action, hash, parent, cost));
                    return true;
                }
                if (full_ && cost >= st_.all_prod().first)
                {
                    // 保持しているどの候補よりもコストが小さくないとき
                    return false;
                }
                int i = 0;
                auto [valid, index] = hash_to_index_.get_index(hash);
                i = index;

                if (valid)
                {
                    int j = hash_to_index_.get(i);
                    if (hash == candidates_[j].hash)
                    {
                        // ハッシュ値が等しいものが存在しているとき
                        if (cost < candidates_[j].cost)
                        {
                            // 更新する場合
                            candidates_[j] = Candidate(action, hash, parent, cost);
                            if (full_)
                            {
                                st_.set(j, {cost, j});
                            }
                            return true;
                        }
                        return false;
                    }
                }
                if (full_)
                {
                    // segment treeが構築されている場合
                    int j = st_.all_prod().second;
                    hash_to_index_.set(i, hash, j);
                    candidates_[j] = Candidate(action, hash, parent, cost);
                    st_.set(j, {cost, j});
                }
                else
                {
                    // segment treeが構築されていない場合
                    hash_to_index_.set(i, hash, candidates_.size());
                    candidates_.emplace_back(Candidate(action, hash, parent, cost));

                    if (candidates_.size() == beam_width)
                    {
                        // 保持している候補がビーム幅分になったとき
                        construct_segment_tree();
                    }
                }
                return true;
            }

            // 選んだ候補を返す
            const vector<Candidate> &select() const
            {
                return candidates_;
            }

            // 実行可能解が見つかったか
            bool have_finished() const
            {
                return !finished_candidates_.empty();
            }

            // 実行可能解に到達する「候補」を返す
            vector<Candidate> get_finished_candidates() const
            {
                return finished_candidates_;
            }

            // 最も評価がよい候補を返す
            Candidate calc_best_candidate()
            {
                int best = 0;
                for (size_t i = 0; i < candidates_.size(); ++i)
                {
                    if (candidates_[i].cost < candidates_[best].cost)
                    {
                        best = i;
                    }
                }
                return candidates_[best];
            }

            void clear()
            {
                candidates_.clear();
                hash_to_index_.clear();
                full_ = false;
            }

        private:
            size_t beam_width;
            vector<Candidate> candidates_;
            HashMap<Hash, int> hash_to_index_;
            bool full_;
            vector<pair<Cost, int>> st_original_;
            MaxSegtree st_;
            vector<Candidate> finished_candidates_;

            void construct_segment_tree()
            {
                full_ = true;
                for (size_t i = 0; i < beam_width; ++i)
                {
                    st_original_[i] = {candidates_[i].cost, i};
                }
                st_ = MaxSegtree(st_original_);
            }
        };

        // ターン毎に候補を管理する
        class MultiSelectors
        {
        public:
            explicit MultiSelectors(const Config &config) : config_(config)
            {
                step_max_ = 1;
            }

            // 候補を追加する
            // ターン数最小化型の問題で、candidateによって実行可能解が得られる場合にのみ finished = true とする
            // step は何ターン後に遷移するかを表す
            bool push(Action action, Cost cost, Hash hash, int parent, bool finished, size_t step)
            {
                while (selectors_.size() < step)
                {
                    selectors_.emplace_back(Selector(config_));
                }
                if (selectors_[step - 1].push(action, cost, hash, parent, finished))
                {
                    if (step > step_max_)
                    {
                        step_max_ = step;
                    }
                    return true;
                }
                return false;
            }

            // expandの直前に呼ぶ
            void reset_step_max()
            {
                step_max_ = 1;
            }

            size_t get_step_max() const
            {
                return step_max_;
            }

            // 次の候補を保持したSelectorを取り出す
            Selector pop_selector()
            {
                Selector ret = move(selectors_.front());
                selectors_.pop_front();
                return ret;
            }

            // Selectorを使い回す
            void push_selector(Selector &&selector)
            {
                selector.clear();
                selectors_.push_back(move(selector));
            }

        private:
            Config config_;
            deque<Selector> selectors_;
            size_t step_max_;
        };

        // 探索木（二重連鎖木）のノード
        struct Node
        {
            Action action;
            Cost cost;
            Hash hash;
            int parent, child, left, right;
            bool active;

            // 根のコンストラクタ
            Node(Action action, Cost cost, Hash hash) : action(action),
                                                        cost(cost),
                                                        hash(hash),
                                                        parent(-1),
                                                        child(-1),
                                                        left(-1),
                                                        right(-1),
                                                        active(true) {}

            // 通常のコンストラクタ
            Node(const Candidate &candidate, int right) : action(candidate.action),
                                                          cost(candidate.cost),
                                                          hash(candidate.hash),
                                                          parent(candidate.parent),
                                                          child(-1),
                                                          left(-1),
                                                          right(right),
                                                          active(true) {}
        };

        // 二重連鎖木に対する操作をまとめたクラス
        class Tree
        {
        public:
            explicit Tree(const State<MultiSelectors> &state, size_t nodes_capacity, const Node &root) : state_(state)
            {
                nodes_.reserve(nodes_capacity);
                root_ = nodes_.push(root);
            }

            // 状態を更新しながら深さ優先探索を行い、次のノードの候補を全てselectorに追加する
            void dfs(MultiSelectors &multi_selectors)
            {
                remove_useless_nodes();
                update_root();

                int v = root_;

                if (!nodes_[v].active)
                {
                    // activeなノードがないとき
                    return;
                }

                while (true)
                {
                    v = move_to_leaf(v);

                    multi_selectors.reset_step_max();
                    state_.expand(v, multi_selectors);
                    while (remove_nodes_.size() < multi_selectors.get_step_max())
                    {
                        remove_nodes_.emplace_back();
                    }
                    remove_nodes_[multi_selectors.get_step_max() - 1].push_back(v);

                    v = move_to_ancestor(v);
                    if (v == root_)
                    {
                        break;
                    }
                }
            }

            // 根からノードvまでのパスを取得する
            vector<Action> get_path(int v)
            {
                // cerr << nodes_.size() << endl;

                vector<Action> path;
                while (nodes_[v].parent != -1)
                {
                    path.push_back(nodes_[v].action);
                    v = nodes_[v].parent;
                }
                reverse(path.begin(), path.end());
                return path;
            }

            // 新しいノードを追加する
            int add_leaf(const Candidate &candidate)
            {
                int parent = candidate.parent;
                int sibling = nodes_[parent].child;
                int v = nodes_.push(Node(candidate, sibling));

                nodes_[parent].child = v;

                if (sibling != -1)
                {
                    nodes_[sibling].left = v;
                }

                // 祖先をactivateする
                int u = parent;
                while (!nodes_[u].active)
                {
                    nodes_[u].active = true;
                    if (u == root_)
                    {
                        break;
                    }
                    u = nodes_[u].parent;
                }

                return v;
            }

        private:
            State<MultiSelectors> state_;
            ObjectPool<Node> nodes_;
            int root_;
            deque<vector<int>> remove_nodes_;

            // 根から一本道の部分は往復しないようにする
            void update_root()
            {
                int child = nodes_[root_].child;
                while (child != -1 && nodes_[child].right == -1)
                {
                    root_ = child;
                    state_.move_forward(nodes_[child].action);
                    child = nodes_[child].child;
                }
            }

            // ノードvの子孫で、最も左にある葉に移動する
            int move_to_leaf(int v)
            {
                int child = nodes_[v].child;
                while (child != -1)
                {
                    // activeなノードが見つかるまで右に移動する
                    while (!nodes_[child].active)
                    {
                        child = nodes_[child].right;
                    }
                    nodes_[v].active = false;
                    v = child;
                    state_.move_forward(nodes_[child].action);
                    child = nodes_[child].child;
                }
                nodes_[v].active = false;
                return v;
            }

            // ノードvの先祖で、右への分岐があるところまで移動する
            int move_to_ancestor(int v)
            {
                while (v != root_)
                {
                    state_.move_backward(nodes_[v].action);

                    // activeなノードが見つかるまで右に移動する
                    int u = nodes_[v].right;
                    while (u != -1)
                    {
                        if (nodes_[u].active)
                        {
                            state_.move_forward(nodes_[u].action);
                            return u;
                        }
                        u = nodes_[u].right;
                    }

                    v = nodes_[v].parent;
                }
                return root_;
            }

            // 不要になったノードを全て削除する
            void remove_useless_nodes()
            {
                if (remove_nodes_.empty())
                {
                    return;
                }
                for (int v : remove_nodes_.front())
                {
                    if (nodes_[v].child == -1)
                    {
                        remove_leaf(v);
                    }
                }

                remove_nodes_.front().clear();

                // 先頭の要素を末尾に移動
                remove_nodes_.push_back(move(remove_nodes_.front()));
                remove_nodes_.pop_front();
            }

            // 不要になった葉を再帰的に削除する
            void remove_leaf(int v)
            {
                while (true)
                {
                    int left = nodes_[v].left;
                    int right = nodes_[v].right;
                    if (left == -1)
                    {
                        int parent = nodes_[v].parent;

                        if (parent == -1)
                        {
                            cerr << "ERROR: root is removed" << endl;
                            exit(-1);
                        }
                        nodes_.pop(v);
                        nodes_[parent].child = right;
                        if (right != -1)
                        {
                            nodes_[right].left = -1;
                            return;
                        }
                        v = parent;
                    }
                    else
                    {
                        nodes_.pop(v);
                        nodes_[left].right = right;
                        if (right != -1)
                        {
                            nodes_[right].left = left;
                        }
                        return;
                    }
                }
            }
        };

        // ビームサーチを行う関数
        vector<Action> beam_search(const Config &config, State<MultiSelectors> state, Node root)
        {
            Tree tree(state, config.nodes_capacity, root);

            // 新しいノード候補の集合
            MultiSelectors multi_selectors(config);

            for (int turn = 0; turn < config.max_turn; ++turn)
            {
                // Euler Tour で selector に候補を追加する
                tree.dfs(multi_selectors);

                Selector selector = multi_selectors.pop_selector();
                if (selector.have_finished())
                {
                    // ターン数最小化型の問題で実行可能解が見つかったとき
                    Candidate candidate = selector.get_finished_candidates()[0];
                    vector<Action> ret = tree.get_path(candidate.parent);
                    ret.push_back(candidate.action);
                    return ret;
                }

                if (turn == config.max_turn - 1)
                {
                    // 最終ターン
                    Candidate candidate = selector.calc_best_candidate();
                    vector<Action> ret = tree.get_path(candidate.parent);
                    ret.push_back(candidate.action);
                    return ret;
                }

                // 新しいノードを追加する
                for (const Candidate &candidate : selector.select())
                {
                    tree.add_leaf(candidate);
                }

                // Selector を使い回す
                multi_selectors.push_selector(move(selector));
            }
            assert(false);
            return {};
        }

        // StateConcept のチェックを構造体内で実施
        static_assert(StateConcept<State<MultiSelectors>, Hash, Cost, Action, MultiSelectors>,
                      "State template must satisfy StateConcept with BeamSearch::Selector");

    }; // BeamSearch

    template <typename StateType, typename CostType, typename ActionType, typename SelectorType>
    concept StateNoHashConcept = CostConcept<CostType> &&
                                 requires(StateType state, SelectorType selector) {
                                     { state.expand(std::declval<int>(), selector) } -> same_as<void>;
                                     { state.move_forward(std::declval<ActionType>()) } -> same_as<void>;
                                     { state.move_backward(std::declval<ActionType>()) } -> same_as<void>;
                                 };

    template <typename Action, CostConcept Cost, template <typename> class State>
    struct BeamSearchNoHash
    {
        // 展開するノードの候補を表す構造体
        struct Candidate
        {
            Action action;
            int parent;
            Cost cost;

            Candidate(Action action, int parent, Cost cost) : action(action),
                                                              parent(parent),
                                                              cost(cost) {}
        };

        // ビームサーチの設定
        struct Config
        {
            int max_turn;
            size_t beam_width;
            size_t nodes_capacity;
        };

        static pair<Cost, int> max_func(pair<Cost, int> a, pair<Cost, int> b)
        {
            if (a.first >= b.first)
            {
                return a;
            }
            else
            {
                return b;
            }
        };
        static pair<Cost, int> min_func()
        {
            return make_pair(numeric_limits<Cost>::min(), -1);
        };

        // 削除可能な優先度付きキュー
        using MaxSegtree = atcoder::segtree<
            pair<Cost, int>,
            max_func,
            min_func>;

        // ノードの候補から実際に追加するものを選ぶクラス
        // ビーム幅の個数だけ、評価がよいものを選ぶ
        // ハッシュ値が一致したものについては、評価がよいほうのみを残す
        class Selector
        {
        public:
            explicit Selector(const Config &config)
            {
                beam_width = config.beam_width;
                candidates_.reserve(beam_width);
                full_ = false;
                st_original_.resize(beam_width);
            }

            // 候補を追加する
            // ターン数最小化型の問題で、candidateによって実行可能解が得られる場合にのみ finished = true とする
            // ビーム幅分の候補をCandidateを追加したときにsegment treeを構築する
            bool push(const Action &action, const Cost cost, const int parent, bool finished)
            {
                if (finished)
                {
                    finished_candidates_.emplace_back(Candidate(action, parent, cost));
                    return true;
                }
                if (full_ && cost >= st_.all_prod().first)
                {
                    // 保持しているどの候補よりもコストが小さくないとき
                    return false;
                }
                int i = 0;
                if (full_)
                {
                    // segment treeが構築されている場合
                    int j = st_.all_prod().second;
                    candidates_[j] = Candidate(action, parent, cost);
                    st_.set(j, {cost, j});
                }
                else
                {
                    // segment treeが構築されていない場合
                    candidates_.emplace_back(Candidate(action, parent, cost));

                    if (candidates_.size() == beam_width)
                    {
                        // 保持している候補がビーム幅分になったとき
                        construct_segment_tree();
                    }
                }
                return true;
            }

            // 選んだ候補を返す
            const vector<Candidate> &select() const
            {
                return candidates_;
            }

            // 実行可能解が見つかったか
            bool have_finished() const
            {
                return !finished_candidates_.empty();
            }

            // 実行可能解に到達する「候補」を返す
            vector<Candidate> get_finished_candidates() const
            {
                return finished_candidates_;
            }

            // 最も評価がよい候補を返す
            Candidate calc_best_candidate()
            {
                int best = 0;
                for (size_t i = 0; i < candidates_.size(); ++i)
                {
                    if (candidates_[i].cost < candidates_[best].cost)
                    {
                        best = i;
                    }
                }
                return candidates_[best];
            }

            void clear()
            {
                candidates_.clear();
                full_ = false;
            }

        private:
            size_t beam_width;
            vector<Candidate> candidates_;
            bool full_;
            vector<pair<Cost, int>> st_original_;
            MaxSegtree st_;
            vector<Candidate> finished_candidates_;

            void construct_segment_tree()
            {
                full_ = true;
                for (size_t i = 0; i < beam_width; ++i)
                {
                    st_original_[i] = {candidates_[i].cost, i};
                }
                st_ = MaxSegtree(st_original_);
            }
        };

        // ターン毎に候補を管理する
        class MultiSelectors
        {
        public:
            explicit MultiSelectors(const Config &config) : config_(config)
            {
                step_max_ = 1;
            }

            // 候補を追加する
            // ターン数最小化型の問題で、candidateによって実行可能解が得られる場合にのみ finished = true とする
            // step は何ターン後に遷移するかを表す
            bool push(Action action, Cost cost, int parent, bool finished, size_t step)
            {
                while (selectors_.size() < step)
                {
                    selectors_.emplace_back(Selector(config_));
                }
                if (selectors_[step - 1].push(action, cost, parent, finished))
                {
                    if (step > step_max_)
                    {
                        step_max_ = step;
                    }
                    return true;
                }
                return false;
            }

            // expandの直前に呼ぶ
            void reset_step_max()
            {
                step_max_ = 1;
            }

            size_t get_step_max() const
            {
                return step_max_;
            }

            // 次の候補を保持したSelectorを取り出す
            Selector pop_selector()
            {
                Selector ret = move(selectors_.front());
                selectors_.pop_front();
                return ret;
            }

            // Selectorを使い回す
            void push_selector(Selector &&selector)
            {
                selector.clear();
                selectors_.push_back(move(selector));
            }

        private:
            Config config_;
            deque<Selector> selectors_;
            size_t step_max_;
        };

        // 探索木（二重連鎖木）のノード
        struct Node
        {
            Action action;
            Cost cost;
            int parent, child, left, right;
            bool active;

            // 根のコンストラクタ
            Node(Action action, Cost cost) : action(action),
                                             cost(cost),
                                             parent(-1),
                                             child(-1),
                                             left(-1),
                                             right(-1),
                                             active(true) {}

            // 通常のコンストラクタ
            Node(const Candidate &candidate, int right) : action(candidate.action),
                                                          cost(candidate.cost),
                                                          parent(candidate.parent),
                                                          child(-1),
                                                          left(-1),
                                                          right(right),
                                                          active(true) {}
        };

        // 二重連鎖木に対する操作をまとめたクラス
        class Tree
        {
        public:
            explicit Tree(const State<MultiSelectors> &state, size_t nodes_capacity, const Node &root) : state_(state)
            {
                nodes_.reserve(nodes_capacity);
                root_ = nodes_.push(root);
            }

            // 状態を更新しながら深さ優先探索を行い、次のノードの候補を全てselectorに追加する
            void dfs(MultiSelectors &multi_selectors)
            {
                remove_useless_nodes();
                update_root();

                int v = root_;

                if (!nodes_[v].active)
                {
                    // activeなノードがないとき
                    return;
                }

                while (true)
                {
                    v = move_to_leaf(v);

                    multi_selectors.reset_step_max();
                    state_.expand(v, multi_selectors);
                    while (remove_nodes_.size() < multi_selectors.get_step_max())
                    {
                        remove_nodes_.emplace_back();
                    }
                    remove_nodes_[multi_selectors.get_step_max() - 1].push_back(v);

                    v = move_to_ancestor(v);
                    if (v == root_)
                    {
                        break;
                    }
                }
            }

            // 根からノードvまでのパスを取得する
            vector<Action> get_path(int v)
            {
                // cerr << nodes_.size() << endl;

                vector<Action> path;
                while (nodes_[v].parent != -1)
                {
                    path.push_back(nodes_[v].action);
                    v = nodes_[v].parent;
                }
                reverse(path.begin(), path.end());
                return path;
            }

            // 新しいノードを追加する
            int add_leaf(const Candidate &candidate)
            {
                int parent = candidate.parent;
                int sibling = nodes_[parent].child;
                int v = nodes_.push(Node(candidate, sibling));

                nodes_[parent].child = v;

                if (sibling != -1)
                {
                    nodes_[sibling].left = v;
                }

                // 祖先をactivateする
                int u = parent;
                while (!nodes_[u].active)
                {
                    nodes_[u].active = true;
                    if (u == root_)
                    {
                        break;
                    }
                    u = nodes_[u].parent;
                }

                return v;
            }

        private:
            State<MultiSelectors> state_;
            ObjectPool<Node> nodes_;
            int root_;
            deque<vector<int>> remove_nodes_;

            // 根から一本道の部分は往復しないようにする
            void update_root()
            {
                int child = nodes_[root_].child;
                while (child != -1 && nodes_[child].right == -1)
                {
                    root_ = child;
                    state_.move_forward(nodes_[child].action);
                    child = nodes_[child].child;
                }
            }

            // ノードvの子孫で、最も左にある葉に移動する
            int move_to_leaf(int v)
            {
                int child = nodes_[v].child;
                while (child != -1)
                {
                    // activeなノードが見つかるまで右に移動する
                    while (!nodes_[child].active)
                    {
                        child = nodes_[child].right;
                    }
                    nodes_[v].active = false;
                    v = child;
                    state_.move_forward(nodes_[child].action);
                    child = nodes_[child].child;
                }
                nodes_[v].active = false;
                return v;
            }

            // ノードvの先祖で、右への分岐があるところまで移動する
            int move_to_ancestor(int v)
            {
                while (v != root_)
                {
                    state_.move_backward(nodes_[v].action);

                    // activeなノードが見つかるまで右に移動する
                    int u = nodes_[v].right;
                    while (u != -1)
                    {
                        if (nodes_[u].active)
                        {
                            state_.move_forward(nodes_[u].action);
                            return u;
                        }
                        u = nodes_[u].right;
                    }

                    v = nodes_[v].parent;
                }
                return root_;
            }

            // 不要になったノードを全て削除する
            void remove_useless_nodes()
            {
                if (remove_nodes_.empty())
                {
                    return;
                }
                for (int v : remove_nodes_.front())
                {
                    if (nodes_[v].child == -1)
                    {
                        remove_leaf(v);
                    }
                }

                remove_nodes_.front().clear();

                // 先頭の要素を末尾に移動
                remove_nodes_.push_back(move(remove_nodes_.front()));
                remove_nodes_.pop_front();
            }

            // 不要になった葉を再帰的に削除する
            void remove_leaf(int v)
            {
                while (true)
                {
                    int left = nodes_[v].left;
                    int right = nodes_[v].right;
                    if (left == -1)
                    {
                        int parent = nodes_[v].parent;

                        if (parent == -1)
                        {
                            cerr << "ERROR: root is removed" << endl;
                            exit(-1);
                        }
                        nodes_.pop(v);
                        nodes_[parent].child = right;
                        if (right != -1)
                        {
                            nodes_[right].left = -1;
                            return;
                        }
                        v = parent;
                    }
                    else
                    {
                        nodes_.pop(v);
                        nodes_[left].right = right;
                        if (right != -1)
                        {
                            nodes_[right].left = left;
                        }
                        return;
                    }
                }
            }
        };

        // ビームサーチを行う関数
        vector<Action> beam_search(const Config &config, State<MultiSelectors> state, Node root)
        {
            Tree tree(state, config.nodes_capacity, root);

            // 新しいノード候補の集合
            MultiSelectors multi_selectors(config);

            for (int turn = 0; turn < config.max_turn; ++turn)
            {
                // Euler Tour で selector に候補を追加する
                tree.dfs(multi_selectors);

                Selector selector = multi_selectors.pop_selector();
                if (selector.have_finished())
                {
                    // ターン数最小化型の問題で実行可能解が見つかったとき
                    Candidate candidate = selector.get_finished_candidates()[0];
                    vector<Action> ret = tree.get_path(candidate.parent);
                    ret.push_back(candidate.action);
                    return ret;
                }

                if (turn == config.max_turn - 1)
                {
                    // 最終ターン
                    Candidate candidate = selector.calc_best_candidate();
                    vector<Action> ret = tree.get_path(candidate.parent);
                    ret.push_back(candidate.action);
                    return ret;
                }

                // 新しいノードを追加する
                for (const Candidate &candidate : selector.select())
                {
                    tree.add_leaf(candidate);
                }

                // Selector を使い回す
                multi_selectors.push_selector(move(selector));
            }
            assert(false);
            return {};
        }

        // StateConcept のチェックを構造体内で実施
        static_assert(StateNoHashConcept<State<MultiSelectors>, Cost, Action, MultiSelectors>,
                      "State template must satisfy StateConcept with BeamSearch::Selector");

    }; // BeamSearch

} // namespace skip_beam_library
using namespace skip_beam_library;
#endif