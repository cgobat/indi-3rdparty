#pragma once

#include <QString>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <functional>
#include "SimpleWebSocket.h"
#include "TelescopeData.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <string>

class OriginDiscovery
{
public:
    struct TelescopeInfo {
        std::string ipAddress;
        std::string model;
        time_t lastSeen;
    };

    OriginDiscovery();
    ~OriginDiscovery();

    bool startDiscovery();
    void stopDiscovery();
    void poll();  // Call this regularly from your main poll() loop

    std::vector<TelescopeInfo> getDiscoveredTelescopes() const;
    bool isDiscovering() const { return m_discovering; }
    // Callback type: called when a new telescope is discovered
    using DiscoveryCallback = std::function<void(const TelescopeInfo&)>;
    void setDiscoveryCallback(DiscoveryCallback callback) { m_callback = callback; }

private:
    int m_udpSocket {-1};
    bool m_discovering {false};
    std::vector<TelescopeInfo> m_telescopes;
    time_t m_discoveryStartTime {0};
    DiscoveryCallback m_callback;

    void processPendingDatagrams();
    std::string extractIPAddress(const std::string& datagram);
    std::string extractModel(const std::string& datagram);
};

class OriginBackendSimple
{
public:
    struct TelescopeStatus {
        double altPosition = 0.0;
        double azPosition = 0.0;
        double raPosition = 0.0;
        double decPosition = 0.0;
        bool isConnected = false;
        bool isLogicallyConnected = false;
        bool isCameraLogicallyConnected = false;
        bool isSlewing = false;
        bool isTracking = false;
        bool isParked = false;
        bool isAligned = false;
        bool focuserMoving = false;
        int focuserPosition = 0;
        int focuserMin = 0;
        int focuserMax = 60000;
        int cameraISO = 0;
        double cameraExposure = 0.0;
        QString cameraDetector;
        QString currentOperation = "Idle";
        double temperature = 20.0;
    };

    // Callback types
    using ImageCallback = std::function<void(const QString&, const QByteArray&, double, double, double)>;
    using StatusCallback = std::function<void()>;

    explicit OriginBackendSimple();
    ~OriginBackendSimple();

    // Connection
    bool connectToTelescope(const QString& host, int port = 80);
    void disconnectFromTelescope();
    bool isConnected() const { return m_connected; }
    bool isLogicallyConnected() const { return m_logicallyConnected; }
    void setConnected(bool connected) { m_logicallyConnected = connected; }

    // Camera
    void setCameraConnected(bool connected) { m_cameraConnected = connected; }
    bool isCameraConnected() const { return m_cameraConnected; }

    // Mount operations
    bool gotoPosition(double ra, double dec);
    bool syncPosition(double ra, double dec);
    bool abortMotion();
    bool parkMount();
    bool unparkMount();
    bool setTracking(bool enabled);
    bool slewNorth(bool start);
    bool slewSouth(bool start);
    bool slewEast(bool start);
    bool slewWest(bool start);
    bool isTracking() const;

    // Focuser operations
    bool moveFocuserAbsolute(int position);
    bool moveFocuserRelative(int delta);
    bool abortFocuser();
    bool syncFocuser(int position);
    void requestFocuserStatus();

    // Camera operations
    bool takeSnapshot(double exposure, int iso);
    bool abortExposure();

    // Status
    TelescopeStatus status() const { return m_status; }
    double temperature() const { return m_status.temperature; }
    QString detectorName() const { return m_status.cameraDetector; }

    // Callbacks
    void setImageCallback(ImageCallback cb) { m_imageCallback = cb; }
    void setStatusCallback(StatusCallback cb) { m_statusCallback = cb; }

    // Polling - call this from INDI TimerHit()
    void poll();

private:
    SimpleWebSocket *m_webSocket;
    bool m_autoReconnect {true};
    QString m_lastConnectedHost;
    int m_lastConnectedPort {80};

    bool reconnectWebSocket();
    void setAutoReconnect(bool enable);

    QString m_connectedHost;
    int m_connectedPort;
    bool m_connected;
    bool m_logicallyConnected;
    bool m_cameraConnected;

    TelescopeStatus m_status;
    TelescopeData m_telescopeData;
    int m_nextSequenceId;
    qint64 m_lastMountStatusRequestMs {0};
    qint64 m_lastFocuserStatusRequestMs {0};
    qint64 m_lastCameraStatusRequestMs {0};
    qint64 m_lastCameraInfoRequestMs {0};

    // Callbacks
    ImageCallback m_imageCallback;
    StatusCallback m_statusCallback;
    QByteArray downloadImageSync(const QString& url);
    QString m_pendingImagePath;
    // Message handling
    void processMessage(const std::string& message);
    void sendCommand(const QString& command, const QString& destination,
                    const QJsonObject& params = QJsonObject());
    void requestImage(const QString& filePath);

    // Coordinate conversion
    double hoursToRadians(double hours);
    double degreesToRadians(double degrees);
    double radiansToHours(double radians);
    double radiansToDegrees(double radians);
};
