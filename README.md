﻿BinaryEditor BZ
===============

**このBZはmurachueによるforkです。**

オリジナルのBZはc.mosさん、およびtamachanさんによって作成されています。   
tamachanさんによるBZは以下のページから入手できます。

https://code.google.com/p/binaryeditorbz/

---

ここにあるのは魔改造BZでーす。

以下のような機能が1.8.4+α(1.9.0-SuperFileConとも言う)に追加されたような感じです。

* ビットマップビューのパレットをオリジナル(c.mosさん版)に戻した (tamachanさん版のパレットにも変更可能)
* 細かなビットマップエディタのスクロール
* 選択部分を別ファイルに保存
  * 隠し機能でzlib展開後のデータも保存可能
  * ごみデータが末尾にあった場合は選択範囲をzlibストリーム部分ぴったしにする機能もあります
* 検索機能の強化
  * 1バイトの否定検索 (例: 0x00以外の最初のバイトを検索)
  * リテラル検索 (例: ><script を検索(従来ならアドレスジャンプになる))
  * 16進検索時スペースを必須としない
* 選択範囲を16進文字列としてコピー、16進文字列をバイナリに変換して貼り付け
* Unicode化 (Unicode文字をASCII Viewから入力可能)
  * 文字コードがUnicode系の場合にCF_UNICODETEXTなクリップボードデータを貼り付けた場合はUnicodeのコードポイントのまま貼り付けられます
* ビットマップビューなどの表示/非表示時に、カーソル位置に加えて選択範囲も保持
* ビットマップビュー上に16進ビューのカーソル位置を表示
* ビットマップビューで16進ビューのカーソル位置にジャンプ (ビットマップビューのクリックの逆みたいな)
* Ruby/Python2/Python3によるスクリプティング (ちょっと強引だけど)
  * RubyやPython2/3のスクリプティングを使用する際は、それぞれを別途インストールする必要があります。
    (使用しない場合はインストールの必要はありません。)
* インスペクタの強化 (FILETIMEとCRC16/32/adler32/MD5/SHA1/SHA256)
* クリップボードの形式を指定して貼り付け
* 0x00(Null文字)、およびマルチバイト時にillegalなバイトの色付け (「カラー設定」で変更する必要があります)

ソースコードはgithub(ここ)から、バイナリ(exe/dll)は以下のページから入手できます。

https://drive.google.com/folderview?id=0B8LL_vLQl-GhRFNfelNqU25MS28&usp=sharing

上記ページに置いてあるexe/dllは、個人的なこだわりにより、CRTを動的リンクするようにしています。
したがって、bz.exeを実行する前に、Microsoft Visual Studio 2012のVisual C++再頒布可能パッケージ をインストールしておく必要があります。

Microsoft Visual Studio 2012のVisual C++再頒布可能パッケージ は以下のページから入手できます。

http://www.microsoft.com/ja-jp/download/details.aspx?id=30679

もしくは、以下の3つのDLLをなんとかしてbz.exeと同じディレクトリに配置しておいても動きます
……が、ポータブルな感じにしたい場合以外はおすすめしません。

* mfc110u.dll
* MSVCR110.dll
* atl110.DLL

