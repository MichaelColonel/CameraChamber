#ifndef HTTPSERVERDIALOG_H
#define HTTPSERVERDIALOG_H

#include <QDialog>

namespace Ui {
  class HttpServerDialog;
}

class HttpServerDialog : public QDialog
{
  Q_OBJECT

public:
  explicit HttpServerDialog(QWidget *parent = nullptr);
  ~HttpServerDialog();

private:
  Ui::HttpServerDialog *ui;
};

#endif // HTTPSERVERDIALOG_H
