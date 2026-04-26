#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QTableWidget>
#include <QPushButton>
#include <QProgressBar>
#include <QTimer>
#include "../apiclient.h"

class DashboardPage : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardPage(ApiClient *api, QWidget *parent = nullptr);

public slots:
    void refresh();

private slots:
    void onLatestConsensus(QJsonObject data);
    void onLatestRound(QJsonObject data);
    void onConsensusStats(QJsonObject data);
    void onCurrentRound(QJsonObject data);
    void onRegistryStats(QJsonObject data);
    void onError(QString endpoint, QString error);

private:
    void setupUi();
    QGroupBox *makeStatCard(const QString &title, QLabel *&valueLabel,
                            const QString &color);

    ApiClient *m_api;
    QTimer    *m_autoRefresh;

    QLabel *m_consensusBig;
    QLabel *m_currentRoundLabel2;
    QLabel *m_participantsCountLabel;
    QLabel *m_consensusValueLabel;
    QLabel *m_consensusTempLabel;
    QLabel *m_trustedCountLabel;
    QLabel *m_faultyCountLabel;
    QLabel *m_currentRoundLabel;
    QLabel *m_totalRoundsLabel;
    QLabel *m_totalDevicesLabel;
    QLabel *m_timeRemainingLabel;
    QLabel *m_minSensorsLabel;
    QLabel *m_thresholdLabel;
    QLabel *m_lastUpdatedLabel;
    QLabel *m_statusLabel;
    QLabel *m_lastRoundStatusLabel;
    QLabel *m_lastRoundIdLabel;

    QTableWidget *m_participantsTable;

    QProgressBar *m_roundProgress;

    int m_consensusWindow  = 3600;
    int m_timeRemaining    = 0;
};

#endif 
