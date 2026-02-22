\# VideoScreen



Windows用の動画再生スクリーンセーバーです。

もともと、WindowsNTなど古いWindows用に作成して長らく放置していました。

参考：
[https://forest.watch.impress.co.jp/article/2004/06/11/okiniiri.html]



その後、Windows10用にアップデートしましたが、動画APIがMCIという古い方式でした。

今回、Windows11用にアップデートするのに際して、MCIからWidows Media Playerという新しい動画APIに変更しています。

未完成ですが、なんとなく動作するようになってきました。



現在わかっている不具合：

-  2画面構成の場合に、動画が2画面にまたがって表示される。

　プライマリへの表示にする予定です。





## 使い方

1. `VS.sln` をVisual Studioで開きます。

2. ビルドして作成されたVS.SCRファイルを右クリックしてインストールしてください。

※後日ちゃんとした使い方を公開する予定です。



## ファイル構成

 - `Vs.c`: メインロジック

 - `Setup.c`: セットアッププログラム
