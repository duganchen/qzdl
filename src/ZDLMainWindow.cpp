/*
 * This file is part of qZDL
 * Copyright (C) 2007-2010  Cody Harris
 * 
 * qZDL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <QtWidgets>
#include <QApplication>
#include <QMainWindow>

#include "ZDLInterface.h"
#include "ZDLMainWindow.h"
#include "ZDLConfigurationManager.h"
#include "ZDLInfoBar.h"

extern QApplication *qapp;
extern QString versionString;

ZDLMainWindow::~ZDLMainWindow(){
	QSize sze = this->size();
	QPoint pt = this->pos();
	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();
	if(zconf){
		QString str = QString("%1,%2").arg(sze.width()).arg(sze.height());
		zconf->setValue("zdl.general", "windowsize", str);
		str = QString("%1,%2").arg(pt.x()).arg(pt.y());
		zconf->setValue("zdl.general", "windowpos", str);
	}
	LOGDATAO() << "Closing main window" << endl;
}

QString ZDLMainWindow::getWindowTitle(){
	QString windowTitle = "ZDL";
	windowTitle += " " ZDL_VERSION_STRING " - ";
	ZDLConfiguration *conf = ZDLConfigurationManager::getConfiguration();
	if(conf){
		QString userConfPath = conf->getPath(ZDLConfiguration::CONF_USER);
		QString currentConf = ZDLConfigurationManager::getConfigFileName();
		if(userConfPath != currentConf){
			windowTitle += ZDLConfigurationManager::getConfigFileName();
		}else{
			windowTitle += "user config";
		}
	}else{
		windowTitle += ZDLConfigurationManager::getConfigFileName();
	}
	LOGDATAO() << "Returning main window title " << windowTitle << endl;
	return windowTitle;

}

ZDLMainWindow::ZDLMainWindow(QWidget *parent): QMainWindow(parent){
	LOGDATAO() << "New main window " << DPTR(this) << endl;
	QString windowTitle = getWindowTitle();
	setWindowTitle(windowTitle);

	setWindowIcon(ZDLConfigurationManager::getIcon());


	setContentsMargins(2,2,2,2);
	layout()->setContentsMargins(2,2,2,2);
	QTabWidget *widget = new QTabWidget(this);

	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();
	if(zconf){
		int ok = 0;
		bool qtok = false;
		if(zconf->hasValue("zdl.general", "windowsize")){
			QString size = zconf->getValue("zdl.general", "windowsize", &ok);
			if(size.contains(",")){
				QStringList list = size.split(",");
				int w = list[0].toInt(&qtok);
				if(qtok){
					int h = list[1].toInt(&qtok);
					if(qtok){
						LOGDATAO() << "Resizing to w:" << w << " h:" << h << endl;
						this->resize(QSize(w,h));
					}
				}
			}
		}
		if(zconf->hasValue("zdl.general", "windowpos")){
			QString size = zconf->getValue("zdl.general", "windowpos", &ok);
			if(size.contains(",")){
				QStringList list = size.split(",");
				int x = list[0].toInt(&qtok);
				if(qtok){
					int y = list[1].toInt(&qtok);
					if(qtok){
						LOGDATAO() << "Moving to x:" << x << " y:" << y << endl;
						this->move(QPoint(x,y));
					}
				}
			}

		}
	}


	intr = new ZDLInterface(this);
	settings = new ZDLSettingsTab(this);

	setCentralWidget(widget);
	widget->addTab(intr, "Main");
	widget->addTab(settings, "Settings");
	//I haven't started on this yet :)
	//widget->addTab(new ZDLInterface(this), "Notifications");

	QAction *qact = new QAction(widget);
	qact->setShortcut(Qt::Key_Return);
	widget->addAction(qact);
	connect(qact, &QAction::triggered, this, &ZDLMainWindow::launch);

	qact2 = new QAction(widget);
	qact2->setShortcut(Qt::Key_Escape);
	widget->addAction(qact2);

	connect(qact2, &QAction::triggered, this, &ZDLMainWindow::quit);

	connect(widget, &QTabWidget::currentChanged, this, &ZDLMainWindow::tabChange);
	LOGDATAO() << "Main window created." << endl;
}

void ZDLMainWindow::tabChange(int newTab){
	LOGDATAO() << "Tab changed to " << newTab << endl;
	if(newTab == 0){
		settings->notifyFromParent(NULL);
		intr->readFromParent(NULL);
	}else if (newTab == 1){
		intr->notifyFromParent(NULL);
		settings->readFromParent(NULL);
	}
}

void ZDLMainWindow::quit(){
	LOGDATAO() << "quitting" << endl;
	writeConfig();
	close();
}

void ZDLMainWindow::launch(){
	LOGDATAO() << "Launching" << endl;
	writeConfig();
	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();

	QString exec = getExecutable();
	if (exec.length() < 1){
		QMessageBox::critical(this, "ZDL", "Please select a source port");
		return;
	}
	QStringList args = getArguments();
	if (args.join("").length() < 1){
		return;
	}

	if(exec.contains("\\")){
		exec.replace("\\","/");
	}

	//Find the executable
	QStringList executablePath = exec.split("/");

	//Remove the last item, which will be the .exe
	executablePath.removeLast();

	//Re-create the string
	QString workingDirectory = executablePath.join("/");

	//Resolve the path to an absolute directory
	QDir cwd(workingDirectory);
	workingDirectory = cwd.absolutePath();
	LOGDATAO() << "Working directory: " << workingDirectory << endl;
	// Turns on launch confirmation
	QProcess *proc = new QProcess(this);

	//Set the working directory to that directory
	proc->setWorkingDirectory(workingDirectory);

	proc->setProcessChannelMode(QProcess::ForwardedChannels);
	proc->start(exec, args);
	procerr = proc->error();
	int stat;
	if (zconf->hasValue("zdl.general", "autoclose")){
		QString append = zconf->getValue("zdl.general", "autoclose",&stat);
		if (append == "1" || append == "true"){
			LOGDATAO() << "Asked to exit... closing" << endl;
			close();
		}
	}
}

void ZDLMainWindow::badLaunch(){
	LOGDATAO() << "badLaunch message box" << endl;
	if(procerr == QProcess::FailedToStart){
		QMessageBox::warning(NULL,"Failed to Start", "Failed to launch the application executable.",QMessageBox::Ok,QMessageBox::Ok);
	}else if(procerr == QProcess::Crashed){
		QMessageBox::warning(NULL,"Process Crashed", "The application ended abnormally (usually due to a crash or error).",QMessageBox::Ok,QMessageBox::Ok);
	}else{
		QMessageBox::warning(NULL,"Unknown error", "There was a problem running the application.",QMessageBox::Ok,QMessageBox::Ok);
	}
}

QStringList ZDLMainWindow::getArguments(){
	LOGDATAO() << "Getting arguments" << endl;
	QStringList ourString;
	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();
	ZDLSection *section = NULL;

	QString iwadName = "";

	bool ok;
	int stat;
	int doquotes = 1;

	if(zconf->hasValue("zdl.save", "iwad")){
		QString rc = zconf->getValue("zdl.save", "iwad", &stat);
		if (rc.length() > 0){
			iwadName = rc;
		}else{
			QMessageBox::critical(this, "ZDL", "Please select an IWAD");
			return ourString;
		}
	}else{
		QMessageBox::critical(this, "ZDL", "Please select an IWAD");
		return ourString;
	}

	section = zconf->getSection("zdl.iwads");
	if (section){
		QVector<ZDLLine*> fileVctr;
		section->getRegex("^i[0-9]+n$", fileVctr);

		for(int i = 0; i < fileVctr.size(); i++){
			ZDLLine *line = fileVctr[i];
			if(line->getValue().compare(iwadName) == 0){
				QString var = line->getVariable();
				if(var.length() >= 3){
					var = var.mid(1,var.length()-2);
					QVector<ZDLLine*> nameVctr;
					var = QString("i") + var + QString("f");
					section->getRegex("^" + var + "$",nameVctr);
					if(nameVctr.size() == 1){
						ourString << "-iwad";
						ourString << nameVctr[0]->getValue();
					}
				}
			}
		}
	}

	if (zconf->hasValue("zdl.save", "skill")){
		ourString << "-skill";
		ourString << zconf->getValue("zdl.save", "skill", &stat);
	}

	if (zconf->hasValue("zdl.save", "warp")){
		ourString << "+map";
		ourString << zconf->getValue("zdl.save", "warp", &stat);
	}

	if (zconf->hasValue("zdl.save", "dmflags")){
		ourString << "+set";
		ourString << "dmflags";
		ourString << zconf->getValue("zdl.save", "dmflags", &stat);
	}

	if (zconf->hasValue("zdl.save", "dmflags2")){
		ourString << "+set";
		ourString << "dmflags2";
		ourString << zconf->getValue("zdl.save", "dmflags2", &stat);
	}

	section = zconf->getSection("zdl.save");
	QStringList pwads;
	QStringList dhacked;
	if (section){
		QVector<ZDLLine*> fileVctr;
		section->getRegex("^file[0-9]+$", fileVctr);

		if (fileVctr.size() > 0){
			for(int i = 0; i < fileVctr.size(); i++){
				if(fileVctr[i]->getValue().endsWith(".deh",Qt::CaseInsensitive) || fileVctr[i]->getValue().endsWith(".bex",Qt::CaseInsensitive)){
					dhacked << fileVctr[i]->getValue();
				}else{
					pwads << fileVctr[i]->getValue();
				}
			}
		}
	}

	if(pwads.size() > 0){
		ourString << "-file";
		ourString << pwads;
	}

	if(dhacked.size() > 0){
		ourString << "-deh";
		ourString << dhacked;
	}

	if(zconf->hasValue("zdl.save","gametype")){
		QString tGameType = zconf->getValue("zdl.save","gametype",&stat);
		if(tGameType != "0"){
			if (tGameType == "2"){
				ourString << "-deathmath";
			}
			int players = 0;
			if(zconf->hasValue("zdl.save","players")){
				QString tPlayers = zconf->getValue("zdl.save","players",&stat);
				players = tPlayers.toInt(&ok, 10);
			}
			if(players > 0){
				ourString << "-host";
				ourString << QString::number(players);
			}else if(players == 0){
				if(zconf->hasValue("zdl.save","host")){
					ourString << "-join";
					ourString << zconf->getValue("zdl.save","host",&stat);
				}
			}
			if(zconf->hasValue("zdl.save","fraglimit")){
				ourString << "+set";
				ourString << "fraglimit";
				ourString << zconf->getValue("zdl.save","fraglimit",&stat);

			}
		}
	}



	if(zconf->hasValue("zdl.net","advenabled")){
		QString aNetEnabled = zconf->getValue("zdl.net","advenabled",&stat);
		if(aNetEnabled == "enabled"){
			if(zconf->hasValue("zdl.net","port")){
				ourString << "-port";
				ourString << zconf->getValue("zdl.net","port",&stat);
			}
			if(zconf->hasValue("zdl.net","extratic")){
				QString tExtratic = zconf->getValue("zdl.net","extratic",&stat);
				if(tExtratic == "enabled"){
					ourString << "-extratic";
				}
			}
			if(zconf->hasValue("zdl.net","netmode")){
				QString tNetMode = zconf->getValue("zdl.net","netmode",&stat);
				if(tNetMode == "1"){
					ourString << "-netmode";
					ourString << "0";
				}else if(tNetMode == "2"){
					ourString << "-netmode";
					ourString << "1";
				}
			}
			if(zconf->hasValue("zdl.net","dup")){
				QString tDup = zconf->getValue("zdl.net","dup",&stat);
				if(tDup != "0"){
					ourString << "-dup";
					ourString << tDup;
				}
			}
		}
	}
	if(zconf->hasValue("zdl.general","quotefiles")){
		int ok;
		QString rc = zconf->getValue("zdl.general","quotefiles",&ok);
		if(rc == "disabled"){
			doquotes = 0;
		}
	}
	if(doquotes){
		for(int i = 0; i < ourString.size(); i++){
			if(ourString[i].contains(" ")){
				QString newString = QString("\"") + ourString[i] + QString("\"");
				ourString[i] = newString;
			}
		}
	}

	if (zconf->hasValue("zdl.general", "alwaysadd")){
		ourString << zconf->getValue("zdl.general", "alwaysadd", &stat);
	}

	if (zconf->hasValue("zdl.save", "extra")){
		ourString << zconf->getValue("zdl.save", "extra", &stat);
	}
	LOGDATAO() << "args: " << ourString << endl;
	return ourString;
}

QString ZDLMainWindow::getExecutable(){
	LOGDATAO() << "Getting exec" << endl;
	ZDLConf *zconf = ZDLConfigurationManager::getActiveConfiguration();
	int stat;
	QString portName = "";
	if(zconf->hasValue("zdl.save", "port")){
		ZDLSection *section = zconf->getSection("zdl.ports");
		portName = zconf->getValue("zdl.save", "port", &stat);
		QVector<ZDLLine*> fileVctr;
		section->getRegex("^p[0-9]+n$", fileVctr);

		for(int i = 0; i < fileVctr.size(); i++){
			ZDLLine *line = fileVctr[i];
			if(line->getValue().compare(portName) == 0){
				QString var = line->getVariable();
				if(var.length() >= 3){
					var = var.mid(1,var.length()-2);
					QVector<ZDLLine*> nameVctr;
					var = QString("p") + var + QString("f");
					section->getRegex("^" + var + "$",nameVctr);
					if(nameVctr.size() == 1){
						LOGDATAO() << "Executable: " << nameVctr[0]->getValue() << endl;
						return QString(nameVctr[0]->getValue());
					}
				}
			}
		}
	}
	LOGDATAO() << "No executable" << endl;
	return QString("");
}


//Pass through functions.
void ZDLMainWindow::startRead(){
	LOGDATAO() << "Starting to read configuration" << endl;
	intr->startRead();
	settings->startRead();
	QString windowTitle = getWindowTitle();
	setWindowTitle(windowTitle);
}

void ZDLMainWindow::writeConfig(){
	LOGDATAO() << "Writing configuration" << endl;
	intr->writeConfig();
	settings->writeConfig();
}

QString ZDLMainWindow::getExtraArgs() const
{
	return intr->getExtraArgs();
}

QString ZDLMainWindow::getMode()
{
	return intr->getMode();
}

QString ZDLMainWindow::getHostAddy()
{
	return intr->getHostAddy();
}

QString ZDLMainWindow::getPlayers()
{
	return intr->getPlayers();
}

 QString ZDLMainWindow::getFragLmit()
 {
	 return intr->getFragLmit();
 }

 QString ZDLMainWindow::getDMFlags()
 {
	 return intr->getDMFlags();
 }

 QString ZDLMainWindow::getDMFlags2()
 {
	 return intr->getDMFlags2();
 }

 QString ZDLMainWindow::getAlwaysArgs()
 {
	 return settings->getAlwaysArgs();
 }

 Qt::CheckState ZDLMainWindow::getLaunchClose()
 {
	 return settings->getLaunchClose();
 }

 Qt::CheckState ZDLMainWindow::getShowPaths()
 {
	 return settings->getShowPaths();
 }

 Qt::CheckState ZDLMainWindow::getLaunchZDL()
 {
	 return settings->getLaunchZDL();
 }

 Qt::CheckState ZDLMainWindow::getSavePaths()
 {
	 return settings->getSavePaths();
 }
