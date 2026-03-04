#include "HttpServerDialog.h"
#include "ui_HttpServerDialog.h"

HttpServerDialog::HttpServerDialog(QWidget *parent) :
  QDialog(parent),
  ui(new Ui::HttpServerDialog)
{
  ui->setupUi(this);
}

HttpServerDialog::~HttpServerDialog()
{
  delete ui;
}
