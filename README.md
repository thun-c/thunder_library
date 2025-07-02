## 概要

- [eijirouさんの差分更新ビームサーチライブラリ](https://eijirou-kyopro.hatenablog.com/entry/2024/02/01/115639)をもう少し使いやすくしたつもりのライブラリをつくった
- [yamatsuxさんの分割コンパイルツール](https://github.com/ymatsux/competitive-programming/tree/main/combiner)を参考につくった分割コンパイルツールも同梱しているので、いっしょに使うと便利

## 使い方

### 分割コンパイルツールを使わない場合
新しくつくったcppファイルにlib/skip_beam.cppの全文をコピペし、その下にsample_skip_beam_hash.cpp or sample_skip_beam_nohash.cppを参考に問題固有のコードを書く

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