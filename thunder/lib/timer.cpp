/**************************************************************/
// プログラムの実行時間を計測するライブラリ
// インクルードした時点から計測が開始されるため、
// ユーザ側は何も気にせずtimer.getTime()を呼べば現時点の実行時間がわかる
/**************************************************************/
#pragma once
#ifndef TIMER_HPP
#define TIMER_HPP
#include <bits/stdc++.h>
// 内部のusing namespace std;が他のプログラムを破壊する可能性があるため、
// ライブラリ全体をnamespaceで囲っている。
namespace timer_library
{
    using namespace std;

    //@brief プログラムの実行時間を計測する
    struct Timer
    {
        chrono::system_clock::time_point start;

        Timer()
        {
            start = chrono::system_clock::now();
        }

        void reset()
        {
            start = chrono::system_clock::now();
        }

        //@brief プログラムの実行時間を取得する
        //@return プログラムの実行時間(秒)
        double getTime()
        {
            auto now = chrono::system_clock::now();
            return chrono::duration<double>(now - start).count();
        }
    };
    Timer timer;
} // namespace timer_library
using namespace timer_library;
#endif