/*
 * Copyright 2013 IoriAYANE
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "tweetdialog.h"
#include "settingsdialog.h"
#include "cookiejar.h"

#include <QtCore/QDate>
#include <QtCore/QTime>
#include <QtCore/QString>
#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtWidgets/QMessageBox>
#include <QtWebKitWidgets/QWebFrame>
#include <QtWebKit/QWebElement>

#include <QtCore/QDebug>

#define URL_KANCOLLE "http://www.dmm.com/netgame/social/-/gadgets/=/app_id=854854/"

#define SETTING_FILE_NAME       "settings.ini"
#define SETTING_FILE_FORMAT     QSettings::IniFormat

#define STATUS_BAR_MSG_TIME     5000

class MainWindow::Private
{
public:
    Private(MainWindow *parent);
    ~Private();

private:
    MainWindow *q;

public:
    Ui::MainWindow ui;
    QSettings settings;       //設定管理
    //設定保存データ
    QString savePath;         //保存パス
};

MainWindow::Private::Private(MainWindow *parent)
    : q(parent)
    , settings(SETTING_FILE_NAME, SETTING_FILE_FORMAT)
    , savePath(settings.value("path", "").toString())
{
    ui.setupUi(q);
    ui.webView->page()->networkAccessManager()->setCookieJar(new CookieJar(q));
    connect(ui.capture, &QAction::triggered, q, &MainWindow::captureGame);
    connect(ui.reload, &QAction::triggered, ui.webView, &QWebView::reload);
    connect(ui.exit, &QAction::triggered, q, &MainWindow::close);

    //設定ダイアログ表示
    connect(ui.preferences, &QAction::triggered, [this]() {
        SettingsDialog dlg(q);
        dlg.setSavePath(savePath);
        if (dlg.exec()) {
            //設定更新
            savePath = dlg.savePath();
        }
    });

    //アバウト
    connect(ui.about, &QAction::triggered, [this]() {
        QMessageBox::about(q, MainWindow::tr("Kan Memo"), MainWindow::tr("Kan Memo -KanMusu Memory-\n\nCopyright 2013 IoriAYANE"));
    });

    connect(ui.webView, &QWebView::loadStarted, [this](){
       ui.progressBar->show();
    });
    //WebViewの読込み完了
    connect(ui.webView, &QWebView::loadFinished, [this](bool ok) {
        if (ok) {
            ui.statusBar->showMessage(MainWindow::tr("complete"), STATUS_BAR_MSG_TIME);
        } else {
            ui.statusBar->showMessage(MainWindow::tr("error"), STATUS_BAR_MSG_TIME);
        }
        ui.progressBar->hide();
    });

    //WebViewの読込み状態
    connect(ui.webView, &QWebView::loadProgress, ui.progressBar, &QProgressBar::setValue);
}

MainWindow::Private::~Private()
{
    //設定保存
    settings.setValue("path", savePath);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , d(new Private(this))
{
    if(d->savePath.length() == 0){
        //設定を促す
        QMessageBox::information(this
                                 , tr("Kan Memo")
                                 , tr("Please select a folder to save the image of KanMusu."));
        d->savePath = SettingsDialog::selectSavePath(this, QDir::homePath());
    }

    //設定
    QWebSettings::globalSettings()->setAttribute(QWebSettings::PluginsEnabled, true);
    QWebSettings::globalSettings()->setAttribute(QWebSettings::JavascriptEnabled, true);
    //艦これ読込み
    d->ui.webView->load(QUrl(URL_KANCOLLE));

    connect(this, &MainWindow::destroyed, [this]() {delete d;});
}

//思い出を残す
void MainWindow::captureGame()
{
    qDebug() << "captureGame";

    QPoint currentPos = d->ui.webView->page()->mainFrame()->scrollPosition();
    d->ui.webView->page()->mainFrame()->setScrollPosition(QPoint(0, 0));

    QWebFrame *frame = d->ui.webView->page()->mainFrame();
    if (frame->childFrames().isEmpty()) {
        d->ui.webView->page()->mainFrame()->setScrollPosition(currentPos);
        d->ui.statusBar->showMessage(tr("failed"), STATUS_BAR_MSG_TIME);
        return;
    }

    frame = frame->childFrames().first();
    QWebElement element = frame->findFirstElement(QStringLiteral("#worldselectswf"));
    if (element.isNull()) {
        d->ui.webView->page()->mainFrame()->setScrollPosition(currentPos);
        d->ui.statusBar->showMessage(tr("failed"), STATUS_BAR_MSG_TIME);
        return;
    }
    QRect geometry = element.geometry();
    geometry.moveTopLeft(geometry.topLeft() + frame->geometry().topLeft());
    qDebug() << geometry;

    QImage img(geometry.size(), QImage::Format_ARGB32);
    QPainter painter(&img);
    //全体を描画
    d->ui.webView->render(&painter, QPoint(0,0), geometry);

    d->ui.webView->page()->mainFrame()->setScrollPosition(currentPos);

    QString path = QStringLiteral("%1/kanmusu_%2.png").arg(d->savePath).arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_hh-mm-ss-zzz")));
    qDebug() << "path:" << path;

    //保存する
    d->ui.statusBar->showMessage(tr("saving to %1...").arg(path), STATUS_BAR_MSG_TIME);
    if(img.save(path)){
        //つぶやくダイアログ
        TweetDialog dlg(this);
        dlg.setImagePath(path);
        dlg.setToken(d->settings.value("token", "").toString());
        dlg.setTokenSecret(d->settings.value("tokenSecret", "").toString());
        dlg.user_id(d->settings.value("user_id", "").toString());
        dlg.screen_name(d->settings.value("screen_name", "").toString());
        dlg.exec();
        d->settings.setValue("token", dlg.token());
        d->settings.setValue("tokenSecret", dlg.tokenSecret());
        d->settings.setValue("user_id", dlg.user_id());
        d->settings.setValue("screen_name", dlg.screen_name());
        //        d->settings.sync();

    }else{
        d->ui.statusBar->showMessage(tr("failed"), STATUS_BAR_MSG_TIME);
    }
}
