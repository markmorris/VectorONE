#include "takeTracker.h"
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QElapsedTimer>
#include "sceneView.h"

QMatrix4x4 TakeTracker::WorldFromPose(cv::Mat Pose)
{
	QMatrix4x4 result;

	cv::Mat camR = cv::Mat(3, 3, CV_64F);
	cv::Mat camT = cv::Mat(3, 1, CV_64F);

	for (int iY = 0; iY < 3; ++iY)
	{
		for (int iX = 0; iX < 3; ++iX)
		{
			camR.at<double>(iX, iY) = Pose.at<double>(iX, iY);
		}
	}

	for (int iX = 0; iX < 3; ++iX)
	{
		camT.at<double>(iX, 0) = Pose.at<double>(iX, 3);
	}

	cv::Mat trueT = -(camR).t() * camT;

	result(0, 0) = camR.at<double>(0, 0);
	result(1, 0) = camR.at<double>(0, 1);
	result(2, 0) = camR.at<double>(0, 2);
	result(3, 0) = 0;

	result(0, 1) = camR.at<double>(1, 0);
	result(1, 1) = camR.at<double>(1, 1);
	result(2, 1) = camR.at<double>(1, 2);
	result(3, 1) = 0;

	result(0, 2) = camR.at<double>(2, 0);
	result(1, 2) = camR.at<double>(2, 1);
	result(2, 2) = camR.at<double>(2, 2);
	result(3, 2) = 0;

	result(0, 3) = trueT.at<double>(0);
	result(1, 3) = trueT.at<double>(1);
	result(2, 3) = trueT.at<double>(2);
	result(3, 3) = 1;

	qDebug() << "OrigT:" << Pose.at<double>(0, 3) << Pose.at<double>(1, 3) << Pose.at<double>(2, 3);
	qDebug() << "TrueT:" << trueT.at<double>(0) << trueT.at<double>(1) << trueT.at<double>(2);

	trueT = -(camR) * trueT;

	qDebug() << "RestT:" << trueT.at<double>(0) << trueT.at<double>(1) << trueT.at<double>(2);


	return result;
}

cv::Mat TakeTracker::PoseFromWorld(QMatrix4x4 World)
{
	cv::Mat pose = cv::Mat::eye(3, 4, CV_64F);
	pose.at<double>(0, 0) = World(0, 0);
	pose.at<double>(0, 1) = World(1, 0);
	pose.at<double>(0, 2) = World(2, 0);
	//pose.at<double>(0, 3) = 0;
	pose.at<double>(1, 0) = World(0, 1);
	pose.at<double>(1, 1) = World(1, 1);
	pose.at<double>(1, 2) = World(2, 1);
	//pose.at<double>(1, 3) = 0;
	pose.at<double>(2, 0) = World(0, 2);
	pose.at<double>(2, 1) = World(1, 2);
	pose.at<double>(2, 2) = World(2, 2);
	//pose.at<double>(2, 3) = 0;

	cv::Mat trueT = cv::Mat(3, 1, CV_64F);
	trueT.at<double>(0) = World(0, 3);
	trueT.at<double>(1) = World(1, 3);
	trueT.at<double>(2) = World(2, 3);

	cv::Mat camR = cv::Mat(3, 3, CV_64F);
	for (int iY = 0; iY < 3; ++iY)
	{
		for (int iX = 0; iX < 3; ++iX)
		{
			camR.at<double>(iX, iY) = pose.at<double>(iX, iY);
		}
	}

	cv::Mat camT = -(camR) * trueT;

	pose.at<double>(0, 3) = camT.at<double>(0);
	pose.at<double>(1, 3) = camT.at<double>(1);
	pose.at<double>(2, 3) = camT.at<double>(2);
	//pose.at<double>(3, 3) = 1;

	return pose;
}

TakeTracker* TakeTracker::Create(int Id, QString TakeName, uint32_t Serial, QString FilePath, LiveTracker* LiveTracker)
{
	QFile file(FilePath);

	qDebug() << "Tracker: Load" << Id << TakeName << Serial << FilePath;

	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		qDebug() << "Tracker: Load file failed";
		return 0;
	}
	
	QByteArray fileData = file.readAll();
	file.close();
	QJsonObject trackerObj = QJsonDocument::fromJson(fileData).object();
	
	TakeTracker* tracker = new TakeTracker();
	tracker->id = Id;
	tracker->takeName = TakeName;
	tracker->serial = Serial;
	tracker->liveTracker = LiveTracker;
	tracker->name = trackerObj["name"].toString();
	tracker->exposure = trackerObj["exposure"].toInt();
	tracker->iso = trackerObj["iso"].toInt();
	tracker->frameCount = 0;
	tracker->vidPlaybackFrame = 0;
	tracker->mode = 0;

	LiveTracker->loaded = true;
	LiveTracker->id = tracker->id;
	LiveTracker->name = tracker->name;
	LiveTracker->serial = tracker->serial;

	tracker->decoder = new Decoder();
	
	QJsonArray jsonIntrinMat = trackerObj["intrinsic"].toObject()["camera"].toArray();
	QJsonArray jsonIntrinDist = trackerObj["intrinsic"].toObject()["distortion"].toArray();
	QJsonArray jsonExtrinProj = trackerObj["extrinsic"].toObject()["pose"].toArray();
	
	// Default cam calibration.
	/*
	calibDistCoeffs.at<double>(0) = -0.332945;
	calibDistCoeffs.at<double>(1) = 0.12465;
	calibDistCoeffs.at<double>(2) = 0.0020142;
	calibDistCoeffs.at<double>(3) = 0.000755178;
	calibDistCoeffs.at<double>(4) = -0.029228;

	calibCameraMatrix.at<double>(0, 0) = 740;
	calibCameraMatrix.at<double>(1, 0) = 0;
	calibCameraMatrix.at<double>(2, 0) = 0;
	calibCameraMatrix.at<double>(0, 1) = 0;
	calibCameraMatrix.at<double>(1, 1) = 740;
	calibCameraMatrix.at<double>(2, 1) = 0;
	calibCameraMatrix.at<double>(0, 2) = 512;
	calibCameraMatrix.at<double>(1, 2) = 352;
	calibCameraMatrix.at<double>(2, 2) = 1;
	*/

	tracker->distCoefs = cv::Mat::zeros(5, 1, CV_64F);
	tracker->distCoefs.at<double>(0) = jsonIntrinDist[0].toDouble();
	tracker->distCoefs.at<double>(1) = jsonIntrinDist[1].toDouble();
	tracker->distCoefs.at<double>(2) = jsonIntrinDist[2].toDouble();
	tracker->distCoefs.at<double>(3) = jsonIntrinDist[3].toDouble();
	tracker->distCoefs.at<double>(4) = jsonIntrinDist[4].toDouble();

	tracker->camMat = cv::Mat::eye(3, 3, CV_64F);
	tracker->camMat.at<double>(0, 0) = jsonIntrinMat[0].toDouble();
	tracker->camMat.at<double>(1, 0) = jsonIntrinMat[1].toDouble();
	tracker->camMat.at<double>(2, 0) = jsonIntrinMat[2].toDouble();
	tracker->camMat.at<double>(0, 1) = jsonIntrinMat[3].toDouble();
	tracker->camMat.at<double>(1, 1) = jsonIntrinMat[4].toDouble();
	tracker->camMat.at<double>(2, 1) = jsonIntrinMat[5].toDouble();
	tracker->camMat.at<double>(0, 2) = jsonIntrinMat[6].toDouble();
	tracker->camMat.at<double>(1, 2) = jsonIntrinMat[7].toDouble();
	tracker->camMat.at<double>(2, 2) = jsonIntrinMat[8].toDouble();

	cv::Mat pose = cv::Mat::eye(3, 4, CV_64F);
	for (int iY = 0; iY < 4; ++iY)
	{
		for (int iX = 0; iX < 3; ++iX)
		{
			pose.at<double>(iX, iY) = jsonExtrinProj[iY * 3 + iX].toDouble();
		}
	}

	QString maskStr = trackerObj["mask"].toString();
	for (int i = 0; i < maskStr.size(); ++i)
	{
		tracker->mask[i] = maskStr[i].toLatin1() - 48;
	}
	LiveTracker->setMask(tracker->mask);
	memcpy(tracker->decoder->frameMaskData, tracker->mask, sizeof(mask));

	//optCamMat = getOptimalNewCameraMatrix(calibCameraMatrix, calibDistCoeffs, Size(VID_W, VID_H), 0.0, Size(VID_W, VID_H), NULL, false);
	tracker->camMatOpt = cv::getOptimalNewCameraMatrix(tracker->camMat, tracker->distCoefs, cv::Size(VID_W, VID_H), 0.0, cv::Size(VID_W, VID_H), NULL, true);
	tracker->SetPose(pose);

	QString infoFilePath = "project/" + TakeName + "/" + QString::number(Serial) + ".trakvid";
	FILE* vidFile = fopen(infoFilePath.toLatin1(), "rb");

	if (vidFile)
	{
		tracker->mode = 1;

		fseek(vidFile, 0, SEEK_END);
		int dataSize = ftell(vidFile);
		fseek(vidFile, 0, SEEK_SET);
		tracker->takeClipData = new uint8_t[dataSize];
		fread(tracker->takeClipData, dataSize, 1, vidFile);
		fclose(vidFile);

		int wp = 0;
		int rp = 0;
		bool prevFrameWasData = false;
		int prevWritePtr = 0;
		int prevSize = 0;

		tracker->vidFrameData.clear();

		while (rp < dataSize)
		{
			if (rp >= dataSize - 20)
				break;

			uint8_t* md = tracker->takeClipData + rp;
			int size = *(int*)&md[0];
			int type = *(int*)&md[4];
			float avgMasterOffset = *(float*)&md[8];
			int64_t frameId = *(int64_t*)&md[12];
			rp += 20;

			if (rp + size >= dataSize)
				break;

			memcpy(tracker->takeClipData + wp, tracker->takeClipData + rp, size);
			rp += size;

			if (type == 2)
			{
				// Skip data frame, but remember wp for next frame start.
				prevFrameWasData = true;
				prevWritePtr = wp;
				prevSize = size;
			}
			else
			{
				while (tracker->frameCount < frameId)
				{
					// Dummy Frame.
					VidFrameData vfdDummy = {};
					vfdDummy.type = 3;
					vfdDummy.index = tracker->frameCount++;
					vfdDummy.size = 0;
					vfdDummy.bufferPosition = 0;
					tracker->vidFrameData.push_back(vfdDummy);
				}

				VidFrameData vfd = {};
				vfd.type = type;
				vfd.index = tracker->frameCount++;
				vfd.size = size;
				vfd.bufferPosition = wp;

				if (prevFrameWasData)
				{
					prevFrameWasData = false;
					vfd.bufferPosition = prevWritePtr;
					vfd.size += prevSize;
				}

				tracker->vidFrameData.push_back(vfd);
			}

			wp += size;
		}
	}

	infoFilePath = "project/" + TakeName + "/" + QString::number(Serial) + ".trakblobs";
	FILE* blobFile = fopen(infoFilePath.toLatin1(), "rb");

	if (blobFile)
	{	
		tracker->mode = 2;

		while (!feof(blobFile))
		{
			int frameSize;
			fread(&frameSize, sizeof(frameSize), 1, blobFile);

			blobDataHeader header;
			fread(&header, sizeof(header), 1, blobFile);
			
			while (tracker->frameCount < header.frameId)
			{
				// Dummy Frame.
				VidFrameData vfdDummy = {};
				vfdDummy.type = 3;
				vfdDummy.index = tracker->frameCount++;
				vfdDummy.size = 0;
				vfdDummy.bufferPosition = 0;
				tracker->vidFrameData.push_back(vfdDummy);
			}

			VidFrameData vfd = {};
			vfd.type = 0;
			vfd.index = tracker->frameCount++;
			vfd.size = 0;
			vfd.bufferPosition = 0;
			
			for (int i = 0; i < header.blobCount; ++i)
			{
				blob b;
				fread(&b, sizeof(b), 1, blobFile);

				Marker2D marker = {};
				marker.pos = QVector2D(b.cX, b.cY);
				marker.bounds = QVector4D(b.minX, b.minY, b.maxX, b.maxY);
				marker.distPos = QVector2D(b.cX, b.cY);
				marker.trackerId = Id;
				marker.markerId = vfd.newMarkers.size();
				vfd.newMarkers.push_back(marker);
			}

			tracker->vidFrameData.push_back(vfd);

			for (int i = 0; i < header.foundRegionCount; ++i)
			{
				region r;
				fread(&r, sizeof(r), 1, blobFile);
			}
			
			//qDebug() << "TRAKBLOBS" << header.frameId << header.blobCount << header.foundRegionCount;
		}

		fclose(blobFile);
	}
	else
	{
		qDebug() << "No blobs file";
	}
	
	qDebug() << "Tracker: Loaded" << Serial;

	//*
	for (int i = 0; i < tracker->vidFrameData.count(); ++i)
	{
		VidFrameData* vfdp = &tracker->vidFrameData[i];
		qDebug() << "Post - Index:" << i << "Frame:" << vfdp->index << "Type:" << vfdp->type;
	}
	//*/
	
	return tracker;
}

TakeTracker::TakeTracker()
{
	drawMarkerFrameIndex = 0;
	takeClipData = 0;
}

TakeTracker::~TakeTracker()
{
}

void TakeTracker::SetCamDist(cv::Mat Cam, cv::Mat Dist)
{
	camMat = Cam.clone();
	distCoefs = Dist.clone();

	camMatOpt = cv::getOptimalNewCameraMatrix(camMat, distCoefs, cv::Size(VID_W, VID_H), 0.0, cv::Size(VID_W, VID_H), NULL, true);

	std::stringstream s;
	s << camMat << "\n\n" << camMatOpt;
	qDebug() << "New cam/dist:" << s.str().c_str();

	projMat = camMatOpt * rtMat;
}

void TakeTracker::SetPose(cv::Mat Pose)
{
	rtMat = Pose.clone();
	worldMat = WorldFromPose(rtMat);

	QVector4D hwPos = worldMat * QVector4D(0, 0, 0, 1);
	worldPos.setX(hwPos.x() / hwPos.w());
	worldPos.setY(hwPos.y() / hwPos.w());
	worldPos.setZ(hwPos.z() / hwPos.w());

	projMat = camMatOpt * rtMat;

	// Rebuild rays for 2D markers.
}

void TakeTracker::DecodeFrame(int FrameIndex, int KeyFrameIndex)
{
	int frameCount = FrameIndex - KeyFrameIndex + 1;
	int procFrame = KeyFrameIndex;

	QElapsedTimer pt;
	pt.start();

	while (frameCount-- > 0)
	{
		if (procFrame < 0 || procFrame >= vidFrameData.count())
		{
			qDebug() << "Out of Range Local Frame Decode";
			return;
		}

		VidFrameData* vfd = &vidFrameData[procFrame++];

		if (vfd->type == 3)
		{
			decoder->ShowBlankFrame();
			decoder->blankFrame = true;
			continue;
		}

		int consumed = 0;
		bool frame = decoder->DoDecodeSingleFrame(takeClipData + vfd->bufferPosition, vfd->size, &consumed);
		decoder->blankFrame = false;

		if (!frame)
			qDebug() << "Failed to Decode Frame";
	}

	_currentDecodeFrameIndex = procFrame;
	int t = pt.elapsed();

	//qDebug() << "Perf" << t;
}

void TakeTracker::AdvanceFrame(int FrameCount)
{
	DecodeFrame(_currentDecodeFrameIndex + FrameCount - 1, _currentDecodeFrameIndex);
}

/*
void TakeTracker::AdvanceFrame(int FrameCount)
{
	int frameCount = FrameCount;
	int procFrame = currentFrameIndex;

	QElapsedTimer pt;
	pt.start();

	while (frameCount > 0)
	{
		int consumed = 0;

		VidFrameData* vfd = &vidFrameData[procFrame++];
		bool frame = decoder->DoDecodeSingleFrame(takeClipData + vfd->bufferPosition, vfd->size, &consumed);

		if (procFrame >= vidFrameData.count())
			return;

		if (frame)
			qDebug() << "SHOULD NEVER BE TRUE";

		if (!frame && vfd->type != 2)
		{
			VidFrameData* vfdn = &vidFrameData[procFrame];
			frame = decoder->DoDecodeSingleFrame(takeClipData + vfdn->bufferPosition, vfdn->size, &consumed);
			frameCount--;			
		}

		//qDebug() << "Decode Frame" << frame << consumed;
	}

	currentFrameIndex = procFrame;
	int t = pt.elapsed();

	//qDebug() << "Perf" << t;
}
*/

void TakeTracker::Save()
{
	qDebug() << "Tracker: Save" << id << takeName << name;

	QByteArray jsonBytes = GetProps().GetJSON(true);
	QFile file("project/" + takeName + "/" + QString::number(serial) + ".tracker");

	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
	{
		qDebug() << "Tracker: Save file failed";
		return;
	}

	file.write(jsonBytes);
	file.close();
}

void TakeTracker::Build2DMarkers(int StartFrame, int EndFrame)
{
	qDebug() << "Build 2D Markers" << name;
	
	int localStartFrame = StartFrame;
	int localEndFrame = EndFrame;

	if (localStartFrame < 0) 
		localStartFrame = 0;
	else if (localStartFrame >= vidFrameData.count())
		localStartFrame = vidFrameData.count();

	if (localEndFrame < 0)
		localEndFrame = 0;
	else if (localEndFrame >= vidFrameData.count())
		localEndFrame = vidFrameData.count();

	// Find actual start frame.
	for (int i = localStartFrame; i <= localEndFrame; ++i)
	{
		if (vidFrameData[i].type != 3)
			break;
		else
			localStartFrame++;
	}

	// Find keyframe for start frame.
	int keyFrameIndex = localStartFrame;
	while (keyFrameIndex >= 0)
	{
		if (vidFrameData[keyFrameIndex].type == 1)
		{
			break;
		}
		--keyFrameIndex;
	}

	if (keyFrameIndex == -1)
	{
		qDebug() << "Can't find frames to process";
		return;
	}

	DecodeFrame(localStartFrame, keyFrameIndex);

	for (int i = localStartFrame + 1; i <= localEndFrame; ++i)
	{
		QElapsedTimer tmr;
		tmr.start();
		AdvanceFrame(1);
		qint64 t1 = tmr.nsecsElapsed();
		vidFrameData[i].newMarkers = decoder->ProcessFrameNewMarkers();
		qint64 t2 = tmr.nsecsElapsed();

		float tTotal = (t2 / 1000) / 1000.0;
		float tDecode = (t1 / 1000) / 1000.0;
		float tCentroids = ((t2 - t1) / 1000) / 1000.0;

		qDebug() << i << "Markers:" << vidFrameData[i].newMarkers.count() << "Time:" << tTotal << tDecode << tCentroids;
	}

	UndistortMarkers(StartFrame, EndFrame);
}

void TakeTracker::UndistortMarkers(int StartFrame, int EndFrame)
{
	int localStartFrame = StartFrame;
	int localEndFrame = EndFrame;

	if (localStartFrame < 0)
		localStartFrame = 0;
	else if (localStartFrame >= vidFrameData.count())
		localStartFrame = vidFrameData.count();

	if (localEndFrame < 0)
		localEndFrame = 0;
	else if (localEndFrame >= vidFrameData.count())
		localEndFrame = vidFrameData.count();

	for (int i = localStartFrame; i <= localEndFrame; ++i)
	{
		// Undistort
		if (vidFrameData[i].newMarkers.size() > 0)
		{
			cv::Mat_<cv::Point2f> matPoint(1, vidFrameData[i].newMarkers.size());
			for (int j = 0; j < vidFrameData[i].newMarkers.size(); ++j)
				matPoint(j) = cv::Point2f(vidFrameData[i].newMarkers[j].distPos.x(), vidFrameData[i].newMarkers[j].distPos.y());

			cv::Mat matOutPoints;
			// NOTE: Just use the opt calib matrix.
			cv::undistortPoints(matPoint, matOutPoints, camMat, distCoefs, cv::noArray(), camMatOpt);
			//cv::undistortPoints(matPoint, matOutPoints, camMatOpt, decoder->_calibDistCoeffs, cv::noArray(), camMatOpt);

			// TODO: Clip markers.
			for (int j = 0; j < matOutPoints.size().width; ++j)
			{
				cv::Point2f p = matOutPoints.at<cv::Point2f>(j);

				// TODO: Check for bounds clipping, need to remove marker.
				//if (p.x >= 0 && p.x < VID_W && p.y >= 0 && p.y < VID_H)
				{
					vidFrameData[i].newMarkers[j].pos = QVector2D(p.x, p.y);
				}
			}
		}
	}
}

void TakeTracker::BuildRays(int StartFrame, int EndFrame)
{
	UndistortMarkers(StartFrame, EndFrame);

	int localStartFrame = StartFrame;
	int localEndFrame = EndFrame;

	if (localStartFrame < 0)
		localStartFrame = 0;
	else if (localStartFrame >= vidFrameData.count())
		localStartFrame = vidFrameData.count();

	if (localEndFrame < 0)
		localEndFrame = 0;
	else if (localEndFrame >= vidFrameData.count())
		localEndFrame = vidFrameData.count();

	std::vector<cv::Point2f> points;
	std::vector<cv::Point3f> elines;

	cv::Matx33d m33((double*)camMatOpt.ptr());
	cv::Matx33d m33Inv = m33.inv();

	for (int i = localStartFrame; i <= localEndFrame; ++i)
	{	
		// Project rays.
		for (int j = 0; j < vidFrameData[i].newMarkers.count(); ++j)
		{
			Marker2D* m = &vidFrameData[i].newMarkers[j];

			cv::Matx31d imgPt(m->pos.x(), m->pos.y(), 1);
			imgPt = m33Inv * imgPt;
			QVector3D d((float)imgPt(0, 0), (float)imgPt(1, 0), (float)imgPt(2, 0));
			d.normalize();

			m->worldRayD = (worldMat * QVector4D(d, 0)).toVector3D();
		}
	}
}

bool TakeTracker::ConvertTimelineToFrame(int TimelineFrame, int* KeyFrameIndex, int* FrameIndex)
{
	int keyFrameIndex = -1;
	int frameIndex = -1;
	int l = 0;
	int r = vidFrameData.count() - 1;

	*KeyFrameIndex = keyFrameIndex;
	*FrameIndex = frameIndex;

	while (l <= r)
	{
		int m = l + (r - l) / 2;

		int mIdx = vidFrameData[m].index;

		if (mIdx == TimelineFrame)
		{
			frameIndex = m;
			break;
		}

		if (mIdx < TimelineFrame)
		{
			l = m + 1;
		}
		else
		{
			r = m - 1;
		}
	}

	keyFrameIndex = frameIndex;

	//qDebug() << "Search:" << TimelineFrame << "FrameIndex:" << frameIndex;

	while (keyFrameIndex >= 0)
	{
		if (vidFrameData[keyFrameIndex].type == 1)
		{
			break;
		}
		--keyFrameIndex;
	}

	if (frameIndex != -1 && keyFrameIndex != -1)
	{
		//if (vidFrameData[frameIndex].type == 3)
			//return false;

		*KeyFrameIndex = keyFrameIndex;
		*FrameIndex = frameIndex;
		return true;
	}
	
	return false;
}

VidFrameData* TakeTracker::GetLocalFrame(int TimelineFrame)
{
	int localFrame = TimelineFrame;
	
	if (localFrame < 0 || localFrame >= vidFrameData.count())
		return 0;

	return &vidFrameData[localFrame];
}

QVector2D TakeTracker::ProjectPoint(QVector3D P)
{
	cv::Mat wp(4, 1, CV_64F);
	wp.at<double>(0) = P.x();
	wp.at<double>(1) = P.y();
	wp.at<double>(2) = P.z();
	wp.at<double>(3) = 1.0;

	cv::Mat imgPt = projMat * wp;

	QVector2D result;
	result.setX(imgPt.at<double>(0) / imgPt.at<double>(2));
	result.setY(imgPt.at<double>(1) / imgPt.at<double>(2));

	return result;
}

TrackerProperties TakeTracker::GetProps()
{
	TrackerProperties result;

	result.name = name;
	memcpy(result.maskData, mask, sizeof(mask));
	result.exposure = exposure;
	result.iso = iso;
	result.distCoefs = distCoefs.clone();
	result.camMat = camMat.clone();
	result.rtMat = rtMat.clone();
	
	return result;
}