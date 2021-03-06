#include "serverThreadWorker.h"

void ServerThreadWorker::OnStart()
{
	qDebug() << "Starting Server Thread Worker" << QThread::currentThreadId();

	takeStartFrameId = 0;

	_tcpServer = new QTcpServer(this);
	connect(_tcpServer, &QTcpServer::newConnection, this, &ServerThreadWorker::OnTcpServerConnectionAvailable);
	_tcpServer->listen(QHostAddress::Any, 8000);
}

TrackerConnection* ServerThreadWorker::LockConnection(int TrackerId)
{
	_connectionMutex.lock();

	TrackerConnection* tracker = _GetTracker(TrackerId);

	if (!tracker)
	{
		_connectionMutex.unlock();
		return 0;
	}

	tracker->Lock();

	return tracker;
}

void ServerThreadWorker::UnlockConnection(TrackerConnection* Tracker)
{
	if (!Tracker)
		return;

	Tracker->Unlock();
	_connectionMutex.unlock();
}

void ServerThreadWorker::OnTcpServerConnectionAvailable()
{
	QTcpSocket* tcpSocket = _tcpServer->nextPendingConnection();

	if (tcpSocket)
	{
		_connectionMutex.lock();
		qDebug() << "New TCP Client" << QThread::currentThreadId();
		tcpSocket->setReadBufferSize(1024 * 1024);
		TrackerConnection* nc = new TrackerConnection(++_nextConnectionId, tcpSocket, this);
		_connections[_nextConnectionId] = nc;
		connect(nc, &TrackerConnection::OnNewFrame, this, &ServerThreadWorker::OnNewFrame);
		connect(nc, &TrackerConnection::OnNewMarkersFrame, this, &ServerThreadWorker::OnNewMarkersFrame);
		connect(nc, &TrackerConnection::OnDisconnected, this, &ServerThreadWorker::OnDisconnected);
		connect(nc, &TrackerConnection::OnInfoUpdate, this, &ServerThreadWorker::OnInfoUpdate);
		_connectionMutex.unlock();

		emit OnTrackerConnected(_nextConnectionId);
	}
}

void ServerThreadWorker::OnDisconnected(TrackerConnection* Tracker)
{
	qDebug() << "Tracker Disconnected";

	_connectionMutex.lock();
	int trackerId = Tracker->id;
	_connections.erase(trackerId);
	delete Tracker;
	_connectionMutex.unlock();

	emit OnTrackerDisconnected(trackerId);
}
void ServerThreadWorker::OnInfoUpdate(TrackerConnection* Tracker)
{
	emit OnTrackerInfoUpdate(Tracker->id);
}

void ServerThreadWorker::OnNewFrame(TrackerConnection* Tracker)
{
	emit OnTrackerFrame(Tracker->id);
}

void ServerThreadWorker::OnNewMarkersFrame(TrackerConnection* Tracker)
{
	emit OnTrackerMarkersFrame(Tracker->id);
}

void ServerThreadWorker::OnMaskChange(int ClientId, QByteArray Data)
{
	TrackerConnection* tracker = _GetTracker(ClientId);

	if (tracker)
	{
		memcpy(tracker->props.maskData, Data.data(), sizeof(tracker->props.maskData));

		char asciiMask[64 * 44];

		for (int i = 0; i < 64 * 44; ++i)
		{
			asciiMask[i] = tracker->props.maskData[i] + '0';
		}

		//QByteArray mask((char*)tracker->maskData, sizeof(tracker->maskData));

		QString cmd = QString("sm,") + QString::fromLatin1(asciiMask, sizeof(tracker->props.maskData)) + QString("\n");
		QByteArray data(cmd.toLatin1());

		tracker->socket->write(data);
	}
}

void ServerThreadWorker::OnSendData(int ClientId, QByteArray Data)
{
	if (ClientId == -1)
	{
		for (std::map<int, TrackerConnection*>::iterator it = _connections.begin(); it != _connections.end(); ++it)
			it->second->socket->write(Data);
	}
	else
	{
		TrackerConnection* tracker = _GetTracker(ClientId);
		if (tracker)
		{
			tracker->socket->write(Data);
		}
	}
}

void ServerThreadWorker::OnUpdateTracker(uint32_t SerialId, QByteArray Props)
{
	for (std::map<int, TrackerConnection*>::iterator it = _connections.begin(); it != _connections.end(); ++it)
	{
		TrackerConnection* t = it->second;
		if (t->serial == SerialId)
		{
			t->UpdateProperties(Props);
		}
	}
}

void ServerThreadWorker::OnViewFeed(int ClientId, int StreamMode)
{
	//qDebug() << "View Feed" << ClientId << QThread::currentThreadId();
	//_StopAllTrackerCams();

	TrackerConnection* tracker = _GetTracker(ClientId);
	if (tracker)
	{
		if (StreamMode == 0)
		{
			tracker->socket->write("ec\n");
			tracker->streamMode = 0;
			tracker->streaming = false;
		}
		else if (StreamMode == 1)
		{
			tracker->socket->write("cm,0\n");
			tracker->socket->write("sc\n");
			tracker->streamMode = 1;
			tracker->streaming = true;
		}
		else if (StreamMode == 2)
		{
			tracker->socket->write("cm,1\n");
			tracker->socket->write("sc\n");
			tracker->streamMode = 2;
			tracker->streaming = true;
		}
	}
}

void ServerThreadWorker::InternalRecordingStart()
{
	for (std::map<int, TrackerConnection*>::iterator it = _connections.begin(); it != _connections.end(); ++it)
	{
		it->second->socket->write("sc\n");
	}
}

void ServerThreadWorker::OnResetFrameIds()
{
	takeStartFrameId = 0;
}

void ServerThreadWorker::OnStartRecording(QString TakeName)
{
	qDebug() << "Start Recording";

	takeStartFrameId = 0;

	for (std::map<int, TrackerConnection*>::iterator it = _connections.begin(); it != _connections.end(); ++it)
	{
		//it->second->socket->write("ec\n");
		//it->second->streaming = false;
		//it->second->recording = false;
		it->second->StartRecording(TakeName);
	}
}

void ServerThreadWorker::OnStopRecording()
{
	qDebug() << "Stop Recording";
	for (std::map<int, TrackerConnection*>::iterator it = _connections.begin(); it != _connections.end(); ++it)
	{
		//it->second->socket->write("ec\n");
		it->second->StopRecording();
	}
}

void ServerThreadWorker::OnStartCalibrating(int TrackerId)
{
	for (std::map<int, TrackerConnection*>::iterator it = _connections.begin(); it != _connections.end(); ++it)
	{
		it->second->decoder->findCalibrationSheet = false;
	}

	TrackerConnection* tracker = _GetTracker(TrackerId);
	qDebug() << "Start calibrating tracker " << TrackerId;
	if (tracker)
	{
		tracker->decoder->findCalibrationSheet = true;
	}
}

void ServerThreadWorker::OnStopCalibrating()
{
	qDebug() << "Stop Calibrating";
	for (std::map<int, TrackerConnection*>::iterator it = _connections.begin(); it != _connections.end(); ++it)
	{
		it->second->decoder->findCalibrationSheet = false;
	}
}

TrackerConnection* ServerThreadWorker::_GetTracker(int ClientId)
{
	if (_connections.find(ClientId) == _connections.end())
	{
		return 0;
	}

	return _connections[ClientId];
}

void ServerThreadWorker::_StopAllTrackerCams()
{
	for (std::map<int, TrackerConnection*>::iterator it = _connections.begin(); it != _connections.end(); ++it)
	{
		it->second->socket->write("ec\n");
		it->second->streaming = false;
		it->second->recording = false;
	}
}