# Tarsau - SAU Arsivleme Programi

Bu proje, Sakarya Universitesi Bilgisayar Muhendisligi Bolumu Sistem Programlama dersi kapsaminda gelistirilmis bir arsivleme uygulamasidir.

`tarsau`, Program Linux ortaminda C dili kullanilarak gelistirilmistir ve komut satiri uzerinden calistirilir.

## Proje Amaci

`tarsau` programinin amaci, birden fazla ASCII metin dosyasini ozel `.sau` arsiv formatinda tek bir dosyada birlestirmek ve gerektiginde bu arsiv dosyasini tekrar acarak dosyalari eski icerikleri ve izinleriyle geri olusturmaktir.

## Ozellikler

- ASCII metin dosyalarini `.sau` formatinda arsivleme
- `.sau` arsiv dosyasini belirtilen dizine acma
- Cikti dosyasi belirtilmezse varsayilan olarak `a.sau` olusturma
- Dosya izinlerini arsivleme ve cikarma sirasinda koruma
- En fazla 32 dosya arsivleme kontrolu
- Toplam dosya boyutu icin 200 MB siniri kontrolu
- Uyumsuz dosya formati kontrolu
- Gecersiz veya bozuk arsiv dosyasi kontrolu
- Ic ice dizin olusturma destegi
- Makefile ile kolay derleme

## Proje Yapisi

```text
b221210066_b211210402/
├── rapor.pdf
└── SauPacker/
    ├── tarsau.c
    ├── Makefile
    └── README.md
```
Proje raporu ana teslim dizininde `rapor.pdf` formatinda yer almaktadir. Kaynak kodlar, `Makefile` ve `README.md` dosyasi ise `SauPacker` klasoru icinde bulunmaktadir.

## Derleme

Program `Makefile` kullanilarak derlenmektedir. Kaynak kodlar `SauPacker` klasoru icinde bulundugu icin once bu klasore girilir:

```bash
cd SauPacker
```

Programi derlemek icin asagidaki komut calistirilir:

```bash
make
```

Bu islem sonucunda `tarsau` adli calistirilabilir dosya olusturulur.

Derleme sonucu olusan dosyalar su sekilde kontrol edilebilir:

```bash
ls
```

Olusturulan calistirilabilir dosyayi ve `.sau` arsiv dosyalarini temizlemek icin:

```bash
make clean
```

## GitHub

Proje gelistirme sureci GitHub uzerinden yurutulmustur.

```text
https://github.com/demiris20/SauPacker
```
