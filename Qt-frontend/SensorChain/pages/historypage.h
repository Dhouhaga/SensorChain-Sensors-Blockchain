#ifndef HISTORYPAGE_H
#define HISTORYPAGE_H

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTextEdit>
#include <QTabWidget>
#include <QTreeWidget>
#include "../apiclient.h"

class HistoryPage : public QWidget
{
    Q_OBJECT

public:
    explicit HistoryPage(ApiClient *api, QWidget *parent = nullptr);

public slots:
    void refresh();

private slots:
    void onAllRounds(QJsonArray rounds);
    void onConsensusRound(QJsonObject round);
    void onEventHistory(QJsonObject events);
    void onError(QString endpoint, QString error);
    void onRoundRowClicked(int row, int col);
    void onLookupRoundClicked();

private:
    void setupUi();
    QWidget *buildRoundsTab();
    QWidget *buildEventsTab();
    void populateRoundsTable(const QJsonArray &rounds);
    void populateRoundDetail(const QJsonObject &round);
    void populateEventTree(const QJsonObject &events);

    ApiClient *m_api;

    //  Rounds tab 
    QTableWidget *m_roundsTable;
    QPushButton  *m_refreshBtn;

    QSpinBox     *m_roundIdInput;
    QPushButton  *m_lookupBtn;

    QLabel       *m_roundDetailTitle;
    QTableWidget *m_sensorBreakdownTable;
    QLabel       *m_roundSummaryLabel;

    //  Events tab 
    QTreeWidget  *m_eventTree;
    QPushButton  *m_loadEventsBtn;
    QSpinBox     *m_fromBlockInput;
};

#endif
