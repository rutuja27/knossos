#ifndef PYTHONPROXY_H
#define PYTHONPROXY_H

#include "skeleton/node.h"

#include <QObject>
#include <QList>

/** Actually this class it not really needed. It only hides the SIGNALS from the PythonProxy */
class PythonProxySignalDelegate : public QObject  {
    Q_OBJECT
signals:
    void userMoveSignal(int x, int y, int z, UserMoveType userMoveType, ViewportType viewportType);
};

extern PythonProxySignalDelegate *pythonProxySignalDelegate;

class PythonProxy : public QObject {
    Q_OBJECT
public:
    explicit PythonProxy(QObject *parent = 0);

signals:
    void echo(QString message);

public slots:
    QString getKnossosVersion();
    QString getKnossosRevision();
    QList<int> getOcPixel(QList<int> Dc, QList<int> pxInDc);
    QList<int> getPosition();
    QList<float> getScale();
    void setPosition(int x, int y, int z);
    quint64 readOverlayVoxel(int x, int y, int z);
    bool writeOverlayVoxel(int x, int y, int z, quint64 val);
    char *addrDcOc2Pointer(int x, int y, int z, bool isOc);
    PyObject *PyBufferAddrDcOc2Pointer(int x, int y, int z, bool isOc);
    QByteArray readDc2Pointer(int x, int y, int z);
    int readDc2PointerPos(int x, int y, int z, int pos);
    bool writeDc2Pointer(int x, int y, int z, char *bytes);
    bool writeDc2PointerPos(int x, int y, int z, int pos, int val);
    QByteArray readOc2Pointer(int x, int y, int z);
    quint64 readOc2PointerPos(int x, int y, int z, int pos);
    bool writeOc2Pointer(int x, int y, int z, char *bytes);
    bool writeOc2PointerPos(int x, int y, int z, int pos, quint64 val);
    void processRegionByBufProxy(QList<int> globalFirst, QList<int> size,
                                 quint64 dataPtr, QList<int> strides, bool isWrite);
    void setMovementArea(int minx, int miny, int minz, int maxx, int maxy, int maxz);
    void resetMovementArea();
    QList<int> getMovementArea();
    float getMovementAreaFactor();
    bool loadStyleSheet(const QString &path);
};

#endif // PYTHONPROXY_H
