#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>

class Widget : public QWidget
{
	Q_OBJECT

public:
	Widget(QWidget *parent = nullptr);
	~Widget();

	void test_run();
	void alphabl();

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	enum modes { SSE_MODE = 1, DEF_MODE = 2};

	QImage back;
	QImage front;
	QImage image;

	const int vidX = 800;
	const int vidY = 600;
	const int cycles = 100;

	int render_mode = SSE_MODE;
};
#endif // WIDGET_H
