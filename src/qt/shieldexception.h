// Copyright (c) 2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_SHIELDEXCEPTION_H
#define BITCOIN_QT_SHIELDEXCEPTION_H

#include <util/check.h>
#include <util/system.h>

#include <QApplication>
#include <QMetaObject>
#include <QString>

#include <cassert>

template <typename FunctionType> void shieldException(FunctionType&& f) {
    bool ok = true;
    try {
        f();
    } catch (const NonFatalCheckError& e) {
        PrintExceptionContinue(&e, "Internal error");
        ok = QMetaObject::invokeMethod(qApp, "handleNonFatalException", Qt::DirectConnection, Q_ARG(QString, QString::fromStdString(e.what())));
    } catch (const std::exception& e) {
        PrintExceptionContinue(&e, "Runaway exception");
        ok = QMetaObject::invokeMethod(qApp, "handleRunawayException", Qt::DirectConnection, Q_ARG(QString, QString::fromStdString(e.what())));
    } catch (...) {
        PrintExceptionContinue(nullptr, "Runaway exception");
        ok = QMetaObject::invokeMethod(qApp, "handleRunawayException", Qt::DirectConnection, Q_ARG(QString, QString("Unknown failure occurred.")));
    }
    assert(ok);
}

#endif // BITCOIN_QT_SHIELDEXCEPTION_H
