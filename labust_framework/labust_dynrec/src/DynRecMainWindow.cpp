/*********************************************************************
* Software License Agreement (BSD License)
*
*  Copyright (c) 2010, LABUST, UNIZG-FER
*  All rights reserved.
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above
*     copyright notice, this list of conditions and the following
*     disclaimer in the documentation and/or other materials provided
*     with the distribution.
*   * Neither the name of the LABUST nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
*  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
*  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
*  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
*  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
*  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
*  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
*  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
*  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*********************************************************************/
#include <labust/gui/DynRecMainWindow.hpp>
#include <ui_DynRecMainWindow.h>

#include <boost/bind.hpp>

#include <QDebug>
#include <QPlainTextEdit>

using namespace labust::gui;

DynRecMainWindow::DynRecMainWindow():
		gui(new Ui::DynRecMainWindow())
{
	this->configure();
}

DynRecMainWindow::DynRecMainWindow(const labust::xml::ReaderPtr reader, const std::string id):
		gui(new Ui::DynRecMainWindow())
{
	this->configure(reader,id);
};

DynRecMainWindow::~DynRecMainWindow(){};

void DynRecMainWindow::configure()
{
	gui->setupUi(this);
	qDebug()<<"Configured.";
}

void DynRecMainWindow::configure(const labust::xml::ReaderPtr reader, const std::string id)
{
	this->configure();
}

DynRecMainWindow::GUICommands::CPtr DynRecMainWindow::getGUICommands()
{
	GUICommands::Ptr commands(new GUICommands());
	//We can do that since the call is thread safe.
	commands->addNewMessageCallback(boost::bind(&DynRecMainWindow::onNewMessage,this,_1,_2));

	return commands;
}

void DynRecMainWindow::on_newVariableName_returnPressed()
{
	this->mediator->registerVariable(gui->newVariableName->text().toUtf8().constData());
	gui->VariableTabs->insertTab(0,makeNewTab(), gui->newVariableName->text());
	gui->newVariableName->clear();
}

QWidget* DynRecMainWindow::makeNewTab()
{
	QGridLayout* layout(new QGridLayout());
	QPlainTextEdit* text(new QPlainTextEdit());
	layout->addWidget(text);

	QWidget* newTab(new QWidget());
	newTab->setLayout(layout);

	return newTab;
}

bool DynRecMainWindow::onNewMessage(const std::string& name, const std::string& data)
{
	boost::mutex::scoped_lock lock(newMessageSync);
	qDebug()<<("New message " + name + ": " + data).c_str();
	return true;
}



