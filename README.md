## 概要

- [eijirouさんの差分更新ビームサーチライブラリ](https://eijirou-kyopro.hatenablog.com/entry/2024/02/01/115639)をもう少し使いやすくしたつもりのライブラリをつくった
- [yamatsuxさんの分割コンパイルツール](https://github.com/ymatsux/competitive-programming/tree/main/combiner)を参考につくった分割コンパイルツールも同梱しているので、いっしょに使うと便利

## 使い方

### 分割コンパイルツールを使わない場合
新しくつくったcppファイルにlib内の好きなライブラリ（たとえばlib/skip_beam.cpp）の全文をコピペし、その下にsample_skip_beam_hash.cpp or sample_skip_beam_nohash.cppを参考に問題固有のコードを書く

### 分割コンパイルツールを使う場合(要Python環境)

sample_skip_beam_hash.cpp or sample_skip_beam_nohash.cppを参考に問題固有のコードを書き、以下のコマンドを打つ

```bash
./compile.sh あなたのコード名から拡張子を除いたもの
```

すると、
`thunder/combined/combined_あなたのコード名.cpp`と`build/a.out`ができる。
`thunder/combined/combined_あなたのコード名.cpp`は1ファイルで動く状態なので、そのままAtCoderに提出できる。
`build/a.out`

例として、以下のコマンドを打って確かめてみることを推奨

```bash
./compile.sh ahc021_skip_beam_hash
```

## ライブラリ一覧
- edge_beam.cpp
 オイラーツアーの辺を保持する実装の差分更新ビームサーチ。hashによる同一盤面除去をする版としない版が同梱
- skip_beam.cpp
 二重連鎖木による実装の差分更新ビームサーチライブラリ。複数ターン後に飛ぶ機能がついているつもりだったが、バグがあり1ターン後しか指定できない。同じ1ターン遷移のビームサーチならedge_beam.cppのほうが高速なため、使うメリットは実質ない。hashによる同一盤面除去をする版としない版が同梱
- timer.cpp
 時間計測をする。グローバル変数の`Timer timer`をライブラリ内で宣言しているので、ユーザ側は何も宣言せずに`timer_library::timer.getTime()`を呼べば実行開始から何秒経過かしたかがわかる。