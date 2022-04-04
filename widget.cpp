#include "widget.h"
#include "ui_widget.h"
#include <QPainter>
#include <QDebug>
#include <QImage>
#include <emmintrin.h>
#include <immintrin.h>
#include <QKeyEvent>
#include <QTime>

Widget::Widget(QWidget *parent)
	: QWidget(parent)
{
	setWindowTitle(tr("Image loading and scaling example"));
	resize(vidX, vidY);

	image = QImage(QSize(vidX, vidY), QImage::Format_RGBA8888_Premultiplied);
	back  = QImage(QSize(vidX, vidY), QImage::Format_RGBA8888_Premultiplied);
	front = QImage(QSize(vidX, vidY), QImage::Format_RGBA8888_Premultiplied);

	if (!back.load(":/Table.png") || !front.load(":/an.png"))
		qDebug() << "Failed opening image";

	test_run();
}

Widget::~Widget()
{
}

void Widget::test_run()
{
	QTime time;
	time.start();

	alphabl();

	double fps = cycles / ((double)time.elapsed()/1000.0);
	qDebug() << "FPS : " << fps << "CYCLES : " << cycles;

	repaint();
}

void Widget::alphabl()
{
	const char I = 255u, Z = 0x80u;
	const __m128i   _0 =                    _mm_set_epi8 (0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0);
	const __m128i _255 = _mm_cvtepu8_epi16 (_mm_set_epi8 (I,I,I,I, I,I,I,I, I,I,I,I, I,I,I,I));

	if (!back.bits() || !front.bits() || !image.bits()) {
		qDebug() << "image nullptr bits buffer";
		return;
	}

	for (int cycle_cnt = 0; cycle_cnt < cycles; cycle_cnt++) {
	if (render_mode == SSE_MODE) {
		for (int y = 0; y < 600; y++)
		for (int x = 0; x < 800; x += 4) {
			//-----------------------------------------------------------------------
			//       15 14 13 12   11 10  9  8    7  6  5  4    3  2  1  0
			// fr = [r3 g3 b3 a3 | r2 g2 b2 a2 | r1 g1 b1 a1 | r0 g0 b0 a0]
			//-----------------------------------------------------------------------

			__m128i fr = _mm_load_si128 ((__m128i*) &(front.bits()[y*vidX*4+x*4]));                   // fr = front[y][x]
			__m128i bk = _mm_load_si128 ((__m128i*) &(back.bits() [y*vidX*4+x*4]));

			//-----------------------------------------------------------------------
			//       15 14 13 12   11 10  9  8    7  6  5  4    3  2  1  0
			// fr = [a3 r3 g3 b3 | a2 r2 g2 b2 | a1 r1 g1 b1 | a0 r0 g0 b0]
			//        \  \  \  \    \  \  \  \   xx xx xx xx   xx xx xx xx
			//         \  \  \  \    \  \  \  \.
			//          \  \  \  \    '--+--+--+-------------+--+--+--.
			//           '--+--+--+------------+--+--+--.     \  \  \  \.
			//                                  \  \  \  \     \  \  \  \.
			// FR = [-- -- -- -- | -- -- -- -- | a3 r3 g3 b3 | a2 r2 g2 b2]
			//-----------------------------------------------------------------------

			__m128i FR = (__m128i) _mm_movehl_ps ((__m128) _0, (__m128) fr);       // FR = (fr >> 8*8)
			__m128i BK = (__m128i) _mm_movehl_ps ((__m128) _0, (__m128) bk);

			//-----------------------------------------------------------------------
			//       15 14 13 12   11 10  9  8    7  6  5  4    3  2  1  0
			// fr = [a3 r3 g3 b3 | a2 r2 g2 b2 | a1 r1 g1 b1 | a0 r0 g0 b0]
			//       xx xx xx xx   xx xx xx xx                 /  /   |  |
			//                                         _______/  /   /   |
			//            ...   ...     ...           /     ____/   /    |
			//           /     /       /             /     /       /     |
			// fr = [-- a1 -- r1 | -- g1 -- b1 | -- a0 -- r0 | -- g0 -- b0]
			//-----------------------------------------------------------------------
			fr = _mm_cvtepu8_epi16 (fr);                                               // fr[i] = (WORD) fr[i]
			FR = _mm_cvtepu8_epi16 (FR);

			bk = _mm_cvtepu8_epi16 (bk);
			BK = _mm_cvtepu8_epi16 (BK);

			//-----------------------------------------------------------------------
			//       15 14 13 12   11 10  9  8    7  6  5  4    3  2  1  0
			// fr = [-- a1 -- r1 | -- g1 -- b1 | -- a0 -- r0 | -- g0 -- b0]
			//          |___________________        |___________________
			//          |     \      \      \       |     \      \      \.
			// a  = [-- a1 -- a1 | -- a1 -- a1 | -- a0 -- a0 | -- a0 -- a0]
			//-----------------------------------------------------------------------

			static const __m128i moveA = _mm_set_epi8 (Z, 14, Z, 14, Z, 14, Z, 14,
								    Z, 6, Z, 6, Z, 6, Z, 6);
			__m128i a = _mm_shuffle_epi8 (fr, moveA);                                // a [for r0/b0/b0...] = a0...
			__m128i A = _mm_shuffle_epi8 (FR, moveA);

			//-----------------------------------------------------------------------
			fr = _mm_mullo_epi16 (fr, a);                                           // fr *= a
			FR = _mm_mullo_epi16 (FR, A);

			bk = _mm_mullo_epi16 (bk, _mm_sub_epi16 (_255, a));                                  // bk *= (255-a)
			BK = _mm_mullo_epi16 (BK, _mm_sub_epi16 (_255, A));

			__m128i sum = _mm_add_epi16 (fr, bk);                                       // sum = fr*a + bk*(255-a)
			__m128i SUM = _mm_add_epi16 (FR, BK);

			//-----------------------------------------------------------------------
			//        15 14 13 12   11 10  9  8    7  6  5  4    3  2  1  0
			// sum = [A1 a1 R1 r1 | G1 g1 B1 b1 | A0 a0 R0 r0 | G0 g0 B0 b0]
			//         \     \       \     \       \_____\_______\_____\.
			//          \_____\_______\_____\______________    \  \  \  \.
			//                                    \  \  \  \    \  \  \  \.
			// sum = [-- -- -- -- | -- -- -- -- | A1 R1 G1 B1 | A0 R0 G0 B0]
			//-----------------------------------------------------------------------

			static const __m128i moveSum = _mm_set_epi8 (Z, Z, Z, Z, Z, Z, Z, Z,
								     15, 13, 11, 9, 7, 5, 3, 1);
			sum = _mm_shuffle_epi8 (sum, moveSum);                                      // sum[i] = (sium[i] >> 8) = (sum[i] / 256)
			SUM = _mm_shuffle_epi8 (SUM, moveSum);

			//-----------------------------------------------------------------------
			//          15 14 13 12   11 10  9  8    7  6  5  4    3  2  1  0
			// sum   = [-- -- -- -- | -- -- -- -- | a1 r1 g1 b1 | a0 r0 g0 b0] ->-.
			// sumHi = [-- -- -- -- | -- -- -- -- | a3 r3 g3 b3 | a2 r2 g2 b2]    |
			//                                      /  /  /  /    /  /  /  /      V
			//             .--+--+--+----+--+--+--++--+--+--+----+--+--+--'       |
			//            /  /  /  /    /  /  /  /    ____________________________/
			//           /  /  /  /    /  /  /  /    /  /  /  /    /  /  /  /
			// color = [a3 r3 g3 b3 | a2 r2 g2 b2 | a1 r1 g1 b1 | a0 r0 g0 b0]
			//-----------------------------------------------------------------------

			__m128i color = (__m128i) _mm_movelh_ps ((__m128) sum, (__m128) SUM);  // color = (sumHi << 8*8) | sum

			_mm_storeu_si128 ((__m128i*) &(image.bits()[y*vidX*4 + x*4]), color);
		}
	} else {
		for (int y = 0; y < 600; y++)
		for (int x = 0; x < 800; x++) {

			uchar *fr = front.bits();
			uchar *bk = back.bits();
			uchar *im = image.bits();

			uchar a  = fr[y*vidX*4 + 4*x + 3]; // TODO ->rgbReserved;

		/*R*/	im[y*vidX*4 + 4*x] = ((fr[y*vidX*4 + 4*x]*(a) + bk[y*vidX*4 + 4*x]* (255-a)) >> 8 );
		/*G*/	im[y*vidX*4 + 4*x + 1] = ((fr[y*vidX*4 + 4*x + 1]*(a) + bk[y*vidX*4 + 4*x + 1]* (255-a)) >> 8 );
		/*B*/	im[y*vidX*4 + 4*x + 2] = ((fr[y*vidX*4 + 4*x + 2]*(a) + bk[y*vidX*4 + 4*x + 2]* (255-a)) >> 8 );
		/*A*/	im[y*vidX*4 + 4*x  +3] = 255;
		}
	}
	}
}

void Widget::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);

	QRect target(0, 0, vidX, vidY);
	QPixmap pmap(vidX, vidY);
	pmap.convertFromImage(image);

	painter.drawPixmap(target, pmap, target);
}
