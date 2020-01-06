{% import 'build_misc/macro.tpl' as helper %}
# QEMUモニタを用いたデバッグ

{{ helper.sample_info() }}

## QEMUモニタの起動方法
本サンプル上で`make monitor`を実行する事で、QEMUモニタに接続できます。或いは、QEMUコンテナ上でlocalhost:4445へTCPアクセスする事でも接続できます。

接続すると、以下のようなメッセージが表示されます。
```
QEMU 2.12.0 monitor - type 'help' for more information
(qemu) 
```

## QEMUの終了方法
`q`をタイプする事で、QEMUを終了する事ができます。QEMUの終了は、hakaseとfriendが動作する仮想マシン全体を停止させる事を意味します。

## 簡単なデバッグ

サンプルのfriend.ccは、`hlt`命令を使用し、プロセッサコアを停止させるものです。`hlt`命令は特定の条件下で停止したプロセッサコアを再開させますが、無限ループによってサイド`hlt`命令が呼ばれるようになっています。このコードを実行した時のfriendコアのステータスをQEMUモニタを用いて確認してみましょう。

```cc
// friend.cc
void friend_main() {
  while(true) {
    asm volatile("hlt;");
  }
}
```

まず、 **`cpu 1`を実行し、cpuを切り替えます。** cpu0はhakaseコアに該当し、friendコードのデバッグを行えません。（ちなみに、本サンプルではcpu0以外の全てのコア（cpu1〜cpu4）をfriendに割り当てているため、cpu3等に切り替えても以下の手順を踏む事ができます。）

cpu1に切り替えた後、`info registers`を実行してみましょう。

```
(qemu) info registers
info registers
RAX=0000000040001000 RBX=0000000040006000 RCX=00000000c0000080 RDX=0000000000000000
RSI=000000000000ffff RDI=0000000040001000 RBP=0000000040007fd0 RSP=0000000040007fd0
R8 =0000000000000000 R9 =0000000000000000 R10=0000000000000000 R11=0000000000000000
R12=0000000000000000 R13=0000000000000000 R14=0000000000000000 R15=0000000000000000
RIP=00000000400000ed RFL=00000046 [---Z-P-] CPL=0 II=0 A20=1 SMM=0 HLT=1
ES =0000 0000000000000000 0000ffff 00009300 DPL=0 DS   [-WA]
CS =0010 0000000000000000 00000000 00209a00 DPL=0 CS64 [-R-]
SS =0018 0000000000000000 00000000 00009300 DPL=0 DS   [-WA]
DS =0018 0000000000000000 00000000 00009300 DPL=0 DS   [-WA]
FS =0000 0000000000000000 0000ffff 00009300 DPL=0 DS   [-WA]
GS =0000 0000000000000000 0000ffff 00009300 DPL=0 DS   [-WA]
LDT=0000 0000000000000000 0000ffff 00008200 DPL=0 LDT
TR =0000 0000000000000000 0000ffff 00008b00 DPL=0 TSS64-busy
GDT=     0000000040000128 0000002f
IDT=     0000000000000000 0000ffff
CR0=80000011 CR2=0000000000000000 CR3=0000000040002000 CR4=000000b0
DR0=0000000000000000 DR1=0000000000000000 DR2=0000000000000000 DR3=0000000000000000 
DR6=00000000ffff0ff0 DR7=0000000000000400
EFER=0000000000000500
FCW=037f FSW=0000 [ST=0] FTW=00 MXCSR=00001f80
FPR0=0000000000000000 0000 FPR1=0000000000000000 0000
FPR2=0000000000000000 0000 FPR3=0000000000000000 0000
FPR4=0000000000000000 0000 FPR5=0000000000000000 0000
FPR6=0000000000000000 0000 FPR7=0000000000000000 0000
XMM00=00000000000000000000000000000000 XMM01=0c661b15e7a68b178bc91b378b9cb35c
XMM02=6c902486f6c47297389c1bf5b222cf0d XMM03=00000000000000000000000000000000
XMM04=00000000000000000000000000000000 XMM05=00000000000000000000000000000000
XMM06=00000000000000000000000000000000 XMM07=00000000000000000000000000000000
XMM08=00000000000000000000000000000000 XMM09=00000000000000000000000000000000
XMM10=00000000000000000000000000000000 XMM11=00000000000000000000000000000000
XMM12=00000000000000000000000000000000 XMM13=00000000000000000000000000000000
XMM14=00000000000000000000000000000000 XMM15=00000000000000000000000000000000
```

現在のRIP（インストラクションポインタ）が0x400000edを指している事を示しています。

仮にfriend.ccの通りfriendコアがhltによって停止しているとすれば、RIPはhltの次のインストラクションを指しているはずです。（これはCPUの仕様です）hlt命令は1byteなので、0x400000ecをディスアセンブルすれば、そこにhlt命令が存在するはずです。

```
(qemu) x /10i 0x400000ec
x /10i 0x400000ec
0x400000ec:  f4                       hlt      
0x400000ed:  eb fd                    jmp      0x400000ec
0x400000ef:  90                       nop      
0x400000f0:  66 b8 18 00              movw     $0x18, %ax
0x400000f4:  8e d8                    movl     %eax, %ds
0x400000f6:  8e d0                    movl     %eax, %ss
0x400000f8:  48 c7 c3 00 60 00 40     movq     $0x40006000, %rbx
0x400000ff:  48 c1 fb 0c              sarq     $0xc, %rbx
0x40000103:  48 c1 e3 0c              shlq     $0xc, %rbx
0x40000107:  b8 00 10 00 00           movl     $0x1000, %eax
```

確かに0x40000ecにhlt命令が存在します。

$eip（ripでない事に注意）によってRIPのレジスタ値を参照する事ができるので、以下のようにアドレスの計算を省略する事もできます。

```
(qemu) x /10i $eip-1
x /10i $eip-1
0x400000ec:  f4                       hlt      
0x400000ed:  eb fd                    jmp      0x400000ec
0x400000ef:  90                       nop      
0x400000f0:  66 b8 18 00              movw     $0x18, %ax
0x400000f4:  8e d8                    movl     %eax, %ds
0x400000f6:  8e d0                    movl     %eax, %ss
0x400000f8:  48 c7 c3 00 60 00 40     movq     $0x40006000, %rbx
0x400000ff:  48 c1 fb 0c              sarq     $0xc, %rbx
0x40000103:  48 c1 e3 0c              shlq     $0xc, %rbx
0x40000107:  b8 00 10 00 00           movl     $0x1000, %eax
```

ディスアセンブルでなく、生のバイナリデータを見たい場合は、以下の通りです。

```
(qemu) x /10x $eip-1
x /10x $eip-1
00000000400000ec: 0x90fdebf4 0x0018b866 0xd08ed88e 0x00c3c748
00000000400000fc: 0x48400060 0x480cfbc1 0xb80ce3c1 0x00001000
000000004000010c: 0x04c10ff0 0x00016425
```

ディスアセンブル時に表示されているバイトコードと全く同じバイナリが得られています。バイナリ出力がリトルエンディアンである事には注意してください。

次に、このhlt命令が本当に`friend_main()`内のhltであるのかを調べてみましょう。ここではQEMUモニタは不要です。シェル上で`bin/objdump -d friend.bin`とし、friendバイナリをディスアセンブルしてみましょう。

```
$ bin/objdump -d friend.bin 

friend.bin:     file format elf64-x86-64


Disassembly of section .text:

00000000400000e8 <_Z11friend_mainv>:
    400000e8:	55                   	push   %rbp
    400000e9:	48 89 e5             	mov    %rsp,%rbp
    400000ec:	f4                   	hlt    
    400000ed:	eb fd                	jmp    400000ec <_Z11friend_mainv+0x4>
    400000ef:	90                   	nop

（後略）
```

関数名がマングルされてしまっていますが、`_Z11friend_mainv`が`friend_main()`です。そして、正しく先程の0x400000ecにhlt命令が埋め込まれている事が確認できます。

## 最後に
QEMUモニタの詳細は[このページ](https://en.wikibooks.org/wiki/QEMU/Monitor)が参考になります。また、[公式ドキュメント](https://en.wikibooks.org/wiki/QEMU/Monitor)を参照するのも良いでしょう。
