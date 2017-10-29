#pragma once

#include <QWidget>

#include "take.h"
#include "liveTracker.h"

QT_USE_NAMESPACE

class MainWindow;

struct blob
{
	float minX;
	float minY;
	float maxX;
	float maxY;
	float cX;
	float cY;
};

struct region
{
	uint8_t	id;
	int minX;
	int minY;
	int maxX;
	int maxY;
	int width;
	int height;
	int pixelIdx;
	int pixelCount;
	uint8_t maxLum;
};

struct blobDataHeader
{
	int64_t frameId;
	float avgMasterOffset;	
	int blobCount;
	int regionCount;
	int foundRegionCount;
	int totalTime;
};

class CameraView : public QWidget
{
	Q_OBJECT

public:

	CameraView(QWidget* Parent, MainWindow* Main);

	MainWindow* main;
	
	int			mode;

	QImage		camImage;
	
	// Take view.
	Take*		take;
	int			timelineFrame;
	std::map<int, LiveTracker*>* trackers;
	
	// Global view controls.
	QVector2D	viewTranslate;
	float		viewZoom;

	int hoveredId;

protected:

	void paintEvent(QPaintEvent* Event);
	void mousePressEvent(QMouseEvent* Event);
	void mouseMoveEvent(QMouseEvent* Event);
	void mouseReleaseEvent(QMouseEvent* Event);
	void wheelEvent(QWheelEvent* Event);

private:

	QFont _mainFont;
	QFont _detailFont;
	QFont _largeFont;

	bool		_mouseLeft;
	bool		_mouseRight;
	QPointF		_mouseDownPos;
	QPointF		_mouseMovedPos;
	int			_mouseDownTrackerId;
	int			_editMaskMode;

	QTransform	_vt;

	QPointF _GetVPointF(float X, float Y);
	QPoint _GetVPoint(float X, float Y);

	LiveTracker* _GetTracker(int X, int Y, QVector2D* TrackerSpace = 0);
	void _DeselectTrackers();

	void _ChangeMask(LiveTracker* Tracker, int X, int Y, bool Value);
};