#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QTimer>
#include "apiclient.h"

// Forward declare pages
class DashboardPage;
class DevicesPage;
class SensorsPage;
class AdminPage;
class HistoryPage;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    ApiClient *api() const { return m_api; }

private slots:
    void onNavItemChanged(int index);

private:
    void setupUi();
    void setupStyles();
    void addNavItem(const QString &icon, const QString &label);

    // Core
    ApiClient      *m_api;
    QTimer         *m_refreshTimer;

    // Layout
    QWidget        *m_centralWidget;
    QHBoxLayout    *m_mainLayout;

    // Side nav
    QWidget        *m_navPanel;
    QVBoxLayout    *m_navLayout;
    QLabel         *m_logoLabel;
    QListWidget    *m_navList;
    QLabel         *m_versionLabel;

    // Content area
    QStackedWidget *m_pages;

    // Pages
    DashboardPage  *m_dashboardPage;
    DevicesPage    *m_devicesPage;
    SensorsPage    *m_sensorsPage;
    AdminPage      *m_adminPage;
    HistoryPage    *m_historyPage;
};

#endif // MAINWINDOW_H
