#pragma once
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSpinBox>
#include <QPushButton>
#include <QTextEdit>
#include <QTableWidget>
#include <QSplitter>
#include <QJsonObject>
#include <QJsonArray>
#include "../apiclient.h"

/**
 * @class ConsensusAnalysisPage
 *
 * Shows the full pairwise-disagreement algorithm breakdown for
 * any completed consensus round.
 *
 * Layout:
 *   ┌─────────────────────────────────────────────────────────┐
 *   │  Round selector  [spinbox]  [Load]                      │
 *   ├─────────────────┬───────────────────────────────────────┤
 *   │  Sensor table   │  Step-by-step algorithm text          │
 *   │  addr|val|score │  • inputs                             │
 *   │  |faulty (R/G)  │  • pairwise comparisons               │
 *   │                 │  • threshold formula                   │
 *   │                 │  • safety check                        │
 *   │                 │  • trusted average                     │
 *   ├─────────────────┴───────────────────────────────────────┤
 *   │  Result bar: consensusValue | trusted | faulty | txHash │
 *   └─────────────────────────────────────────────────────────┘
 */
class ConsensusAnalysisPage : public QWidget
{
    Q_OBJECT

public:
    explicit ConsensusAnalysisPage(ApiClient *api, QWidget *parent = nullptr);
    void refresh();

private slots:
    void onLoadRound();
    void onExplainReady(QJsonObject data);
    void onError(const QString &endpoint, const QString &msg);

private:
    void setupUi();
    void setupConnections();
    void populateSensorTable(const QJsonObject &step2);
    void populateAlgorithmText(const QJsonObject &data);
    void populateResultBar(const QJsonObject &result, const QJsonObject &trace);

    ApiClient *m_api;

    // Round selector
    QSpinBox    *m_roundSpin;
    QPushButton *m_loadBtn;

    // Sensor table (left pane)
    QTableWidget *m_sensorTable;

    // Algorithm narrative (right pane)
    QTextEdit *m_algorithmText;

    // Result bar (bottom)
    QLabel *m_resultValue;
    QLabel *m_resultTrusted;
    QLabel *m_resultFaulty;
    QLabel *m_resultStatus;
    QLabel *m_resultTxHash;
    QLabel *m_resultBlock;
    QLabel *m_resultTimestamp;
};
