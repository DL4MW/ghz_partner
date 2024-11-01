#include <QtCore>
#include <stdio.h>
#include "adifmodel.h"

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <time.h>
#include <termios.h>
#include <string.h>
#endif


#ifndef _WIN32
// in Linux there is no kbhit() function, we create one
int kbhit(void) {
    struct termios term, oterm;
    int fd = 0;
    int c = 0;

    tcgetattr(fd, &oterm);
    memcpy(&term, &oterm, sizeof(term));
    term.c_lflag = term.c_lflag & (!ICANON);
    term.c_cc[VMIN] = 0;
    term.c_cc[VTIME] = 1;
    tcsetattr(fd, TCSANOW, &term);

    c = getchar();

    tcsetattr(fd, TCSANOW, &oterm);
    if (c != -1)
        ungetc(c, stdin);
    return ((c != -1) ? 1 : 0);
}
#endif

AdifDataBase db;


double Band2Freq(QString band)
{
    if(band.compare("2M",Qt::CaseInsensitive)==0)return 144;
    if(band.compare("70CM",Qt::CaseInsensitive)==0)return 432;
    if(band.compare("23CM",Qt::CaseInsensitive)==0)return 1296;
    if(band.compare("13CM",Qt::CaseInsensitive)==0)return 2320;
    if(band.compare("9CM",Qt::CaseInsensitive)==0)return 3400;
    return 0;
}

struct QSO{
    QString own,partner,locator,fileName,band;
    QDateTime datetime;
    int nr;
    bool locChecked,noChecked,wasCW;
    QSO(){nr=0;locChecked=false;noChecked=false;wasCW=false;}
};

struct Partner{
    QString call,locator;
    QStringList wkd;
    bool canCW,has23,has13,has9,has2m,has70cm;
    double distance,azimut;
    Partner(){canCW=false;has23=false;has13=false;has9=false;distance=0;azimut=0;has2m=false;has70cm=false;}
};

bool operator<(const Partner p1, const Partner p2){if(p1.call<p2.call)return true;else return false;}


QList<QSO> qsoList;
QMap<QString, QString> kstMap;
QMap<QString, QStringList> callDB;
QList<QString> bNetzAList;
QList<Partner> partnerList;
double homeLat=0,homeLong=0;


FILE *dbgFile=NULL,*reportfile=NULL,*blackListFile=NULL;

int  qrb(double lon1,
                   double lat1,
                   double lon2,
                   double lat2,
                   double *distance,
                   double *azimuth);

int locator2longlat(double *longitude,
                               double *latitude,
                               const char *locator);

char homeLoc[]="JO50kq";

void Report(const char* msg,...)
{
    va_list ap;
    va_start( ap, msg );
    vprintf(msg, ap );
    if(reportfile)vfprintf(reportfile,msg,ap);
    va_end( ap );

}

QStringList ParseCsvFields(const QString &line, const QChar delimiter)
{
    QString temp = line;
    QString field;
    QStringList field_list;

    // regex explaination
    //
    //    /(?:^|,)(\"(?:[^\"]+|\"\")*\"|[^,]*)/g
    //        (?:^|,) Non-capturing group
    //            1st Alternative: ^
    //                ^ assert position at start of the string
    //            2nd Alternative: ,
    //                , matches the character , literally
    //        1st Capturing group (\"(?:[^\"]+|\"\")*\"|[^,]*)
    //            1st Alternative: \"(?:[^\"]+|\"\")*\"
    //                \" matches the character " literally
    //                (?:[^\"]+|\"\")* Non-capturing group
    //                    Quantifier: * Between zero and unlimited times, as many times as possible, giving back as needed [greedy]
    //                    1st Alternative: [^\"]+
    //                        [^\"]+ match a single character not present in the list below
    //                            Quantifier: + Between one and unlimited times, as many times as possible, giving back as needed [greedy]
    //                            \" matches the character " literally
    //                    2nd Alternative: \"\"
    //                        \" matches the character " literally
    //                        \" matches the character " literally
    //                \" matches the character " literally
    //            2nd Alternative: [^,]*
    //                [^,]* match a single character not present in the list below
    //                    Quantifier: * Between zero and unlimited times, as many times as possible, giving back as needed [greedy]
    //                    , the literal character ,
    //        g modifier: global. All matches (don't return on first match)
    //

    QString regex = "(?:^|,)(\"(?:[^\"]+|\"\")*\"|[^,]*)";
    regex.replace(",", delimiter);

    QRegularExpression re(regex);

    if (temp.right(1) == "\n") temp.chop(1);

    QRegularExpressionMatchIterator it = re.globalMatch(temp);

    while (it.hasNext())
    {
        QRegularExpressionMatch match = it.next();
        if (match.hasMatch())
        {
            field = match.captured(1);
            if (field.left(1) == "\"" && field.right(1) == "\"")
                field = field.mid(1, field.length()-2);
            field_list.push_back(field);
        }
    }

    return field_list;
}





void RemoveQuotes(QStringList &list)
{
	for(int i=0;i<list.count();i++){
		QString s=list[i];
		if(s[0]=='"' && s[s.length()-1]=='"'){
			s=s.remove(0,1);
			s.chop(1);
		}
		list[i]=s;
	}

}



// List QSOs in qsoList aus dem edi-File
void ImportEdi(QString fileName)
{
    FILE *file=fopen(fileName.toLocal8Bit().constData(),"rt");
    printf("Import %s\n",fileName.toLocal8Bit().constData());
    if(file){
        char line[100];
        QString sline,mycall,call,qth,band;
        QDateTime date=QDateTime::currentDateTime();
        call="";qth="";
        int cnt=0;
        do{
            fgets(line,99,file);
            if(feof(file))break;
            sline=QString(line).simplified();
            QStringList list=sline.split('=');
            if(list.count()<2)continue;
            if(list[0].compare("PCall",Qt::CaseInsensitive)==0)mycall=list[1];
            if(list[0].compare("PWWLo",Qt::CaseInsensitive)==0)qth=list[1];
            if(list[0].compare("TDate",Qt::CaseInsensitive)==0)date=QDateTime::fromString(list[1],"yyMMdd");
            if(list[0].compare("PBand",Qt::CaseInsensitive)==0){
                band=list[1];
                band=band.simplified();
                band=band.remove(QChar(' '));
                band=band.toUpper();
            }
        }while(!sline.startsWith("[QSORecords",Qt::CaseInsensitive));

        if(!(qth.toUpper()=="JO50KQ" || qth.toUpper()=="JO50LQ")){
            printf("falscher Locator %s\n",qth.toLocal8Bit().constData());
            return;
        }

        if(feof(file)){
            printf("Vorzeitiges Ende, keine QSOs gefunden\n");
            return;
        }
        while(!feof(file)){
            fgets(line,99,file);
            //if(feof(file))break;
            QString s(line);
            s=s.simplified();
            if(s.startsWith("["))break;
            QStringList list=s.split(';');
            if(list.count()<6)continue;
            QString call=list[2].simplified();
            QString qth=list[9].simplified();
            int nr=list[7].simplified().toInt();
            QString rst=list[6].simplified();
            QDateTime date=QDateTime::currentDateTime();
            date=QDateTime::fromString(list[0],"yyMMdd");
            date.setTime(QTime::fromString(list[1].simplified(),"hhmm"));
            if(!call.isEmpty()&&!qth.isEmpty()){
                QSO q;
                q.fileName=fileName;
                q.own=mycall;
                q.partner=call;
                q.nr=nr;
                q.datetime=date;
                q.locator=qth;
                q.band=band;
                q.wasCW=rst.length()>2;
                qsoList.append(q);
                if(dbgFile)fprintf(dbgFile,"add %s %s %s %s %s %d\n",
                                   mycall.toLatin1().constData(),
                                   call.toLatin1().constData(),
                                   qth.toLatin1().constData(),
                                   date.toString(Qt::ISODate).toLatin1().constData(),
                                   band.toLatin1().constData(),
                                   nr);
                cnt++;
            }
        }
        fclose(file);
        printf("%d importiert\n",cnt-1);
        if(reportfile)fprintf(reportfile,"benutze %s mit %d QSOs\n",fileName.toLocal8Bit().constData(),cnt);

    }else printf("Fehler beim Oeffnen %s\n",fileName.toLocal8Bit().constData());

}


int ReadKSTUser()
{
    FILE *file=fopen("./data/kstlist.txt","rt");
    if(!file)return 0;
    char line[255];
    while(!feof(file)){
        fgets(line,199,file);
        if(feof(file))break;
        QString s(line);
        s=s.simplified();
        QStringList list;
        list=list=s.split(' ',QString::SkipEmptyParts);
        if(s.contains("chat>",Qt::CaseInsensitive))continue;
        if(list.count()<3)continue;
        QString call=list[0].simplified();
        call=call.remove('(');
        call=call.remove(')');
        call=call.section('-',0,0);
        QString qth=list[1].simplified();
        int w=10;
        QDateTime date=QDateTime::currentDateTime();
        if(!call.isEmpty()&&!qth.isEmpty()){
            kstMap[call]=qth;
            if(dbgFile)fprintf(dbgFile,"KST %s %s \n",
                               call.toLatin1().constData(),
                               qth.toLatin1().constData());


        }
    }
    printf("KST-chat: %d Calls\n",kstMap.count());
    if(reportfile)fprintf(reportfile,"Import KST-User dump: %d calls\n",kstMap.count());
    fclose(file);
    return kstMap.count();

}

int ReadCallDB()
{
    FILE *file=fopen("./data/calls_loc.dat","rt");
    if(!file)return 0;
    char line[255];
    while(!feof(file)){
        fgets(line,199,file);
        if(feof(file))break;
        QString s(line);
        s=s.simplified();
        QStringList list;
        list=list=s.split(',',QString::SkipEmptyParts);
        if(list.count()<4)continue;
        QString call=list[0].simplified();
        QString qth=list[1].simplified();
        int wichtung=list[3].toInt();
        QDateTime date=QDateTime::currentDateTime();
        if(!call.isEmpty()&&!qth.isEmpty() && wichtung>1){
            callDB[call].append(qth);
            if(dbgFile)fprintf(dbgFile,"callDB %s %s \n",
                               call.toLatin1().constData(),
                               qth.toLatin1().constData());


        }
    }
    Report("Import Call-DB from Locator-DB project: %d calls\n",callDB.count());
    fclose(file);
    return kstMap.count();

}

QString FindCallsInLocator(QString &loc)
{
    QString ret;
    QMap<QString,QStringList>::const_iterator it;
    for(it=callDB.begin();it!=callDB.end();++it){
        if(it.value().contains(loc)){
            ret+=it.key();
            ret+=" ";
        }
    }

    QMap<QString,QString>::const_iterator it1;
    for(it1=kstMap.begin();it1!=kstMap.end();++it1){
        if(it1.value()==loc && !ret.contains(it1.key())){
            ret+=it1.key();
            ret+=" ";
        }
    }

    return ret;
}

bool CallIsInQSOList(QString &call, QString &owner)
{
    QList<QSO>::iterator it,it1;
    for(it=qsoList.begin(); it!=qsoList.end(); ++it) {
        QSO qso=*it;
        if(qso.own == owner)continue;
        if(qso.partner == call)return true;
    }
    return false;

}

int ReadBNetzAList()
{
    FILE *file=fopen("./data/dl_calls.txt","rt");
    if(!file)return 0;
    char line[255];
    int cnt=0;
    while(!feof(file)){
        fgets(line,199,file);
        if(feof(file))break;
        QString s(line);
        s=s.simplified();
        if(!s.isEmpty()){
            bNetzAList.append(s.toUpper());
            cnt++;
        }
    }
    Report("Import DL calls from BNetzA Callsign-List: %d calls\n",cnt);
    fclose(file);
    return cnt;
}

void AddPartner(QString call, QString band, QString locator, QString wkd, bool cw)
{
    bool found=false;
    QList<Partner>::iterator it;
    call=call.toUpper();
    band=band.toUpper();
    wkd=wkd.toUpper();


    //if(! (band.contains("1,3GHZ") || band.contains("2,3GHZ") || band.contains("3,4GHZ")))return;

    for(it=partnerList.begin(); it!=partnerList.end(); ++it) {
        Partner p= *it;
        if(p.call==call){
            found=true;
            if(band=="1,3GHZ")p.has23=true;
            if(band=="2,3GHZ")p.has13=true;
            if(band=="3,4GHZ")p.has9=true;
            if(band=="145MHZ")p.has2m=true;
            if(band=="435MHZ")p.has70cm=true;
            if(cw)p.canCW=true;

            if(!p.wkd.contains(wkd))p.wkd.append(wkd);
            *it=p;
        }
    }

    if(!found){
        Partner p;
        p.call=call;
            found=true;
            if(band=="1,3GHZ")p.has23=true;
            if(band=="2,3GHZ")p.has13=true;
            if(band=="3,4GHZ")p.has9=true;
            if(band=="145MHZ")p.has2m=true;
            if(band=="435MHZ")p.has70cm=true;
            p.locator=locator;
            if(cw)p.canCW=true;
            if(!p.wkd.contains(wkd))p.wkd.append(wkd);
        double longD,latD;
        locator2longlat(&longD,&latD,p.locator.toLocal8Bit().constData());
        qrb(homeLong,homeLat,longD,latD,&p.distance,&p.azimut);
        if(p.distance>10)partnerList.append(p);

        Report("partner %s %s %s\n",call.toLocal8Bit().constData(),band.toLocal8Bit().constData(),locator.toLocal8Bit().constData());

    }


}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    dbgFile=fopen("logcheck.dbg","wt");
    QString rname=QDateTime::currentDateTime().toString("yyMMdd");
    reportfile=fopen((rname+".txt").toLocal8Bit().constData(),"wt");

    printf("Genieriere Partnerliste(n) aus EDI-Logs \nVersion 1.0 Juli 2024\n");
    
    if(dbgFile)fprintf(dbgFile,"Version 1.0 Juli 2024\n");

    locator2longlat(&homeLong,&homeLat,homeLoc);

    if(dbgFile)fprintf(dbgFile,"Home ist %s %f %f\n",homeLoc,homeLong,homeLat);


    int i,j;
    QDir dir("./data");
    QStringList filters;
    dir.setPath("./data");
    filters.clear();

    dir.setPath("./data");
    filters.clear();
    filters << "*.edi";
    QFileInfoList flist=dir.entryInfoList(filters,QDir::Files);

    for(i=0;i<flist.count();i++){
        if(!flist.at(i).isFile())continue;
        ImportEdi(flist.at(i).absoluteFilePath());

    }

    QList<QSO>::iterator it;
    for(it=qsoList.begin(); it!=qsoList.end(); ++it) {
        QSO qso=*it;
        AddPartner(qso.partner,qso.band,qso.locator,qso.own,qso.wasCW);

    }

    qSort(partnerList);

    // Ausgabefiles erzeugen

    FILE *csvFile=fopen("partnerList.csv","wt");
    FILE *mdFile=fopen("partnerList.md","wt");

    if(csvFile)fprintf(csvFile,"Call;CW?;Locator;Entfernung;Winkel;13?;9?\n");
    if(mdFile)fprintf(mdFile,"^ Call ^ CW?  ^  Loc ^ km^ Dir^ 13 ^9 ^\n");

    // Entfernungen und Winkel berechnen
    // Ausgabe als CSV für Excel und md für das Wiki
    QList<Partner>::iterator it1;
    for(it1=partnerList.begin(); it1!=partnerList.end(); ++it1) {
        Partner p=*it1;
        if(p.has23){
            if(csvFile)fprintf(csvFile,"%s;%s;%s;%.0f;%.0f;%s;%s\n",p.call.toLocal8Bit().constData(),
                               p.canCW?"CW":"-",
                               p.locator.toLocal8Bit().constData(),
                               p.distance,
                               p.azimut,
                               p.has13?"x":"-",
                               p.has9?"x":"-");

            if(mdFile)fprintf(mdFile,"| %s | %s | %s | %.0f | %.0f | %s | %s |\n",p.call.toLocal8Bit().constData(),
                              p.canCW?"CW":"-",
                              p.locator.toLocal8Bit().constData(),
                              p.distance,
                              p.azimut,
                              p.has13?"x":"-",
                              p.has9?"x":"-");
        }
    }
    if(csvFile)fclose(csvFile);
    if(mdFile)fclose(mdFile);


   csvFile=fopen("partnerList_ext.csv","wt");
   mdFile=fopen("partnerList_ext.md","wt");

    if(csvFile)fprintf(csvFile,"Call;CW?;Locator;Entfernung;Winkel;13?;9?\n");
    if(mdFile)fprintf(mdFile,"^ Call ^ CW?  ^  Loc ^ km^ Dir^ 13 ^9 ^\n");

    // Entfernungen und Winkel berechnen
    // Ausgabe als CSV für Excel und md für das Wiki
    for(it1=partnerList.begin(); it1!=partnerList.end(); ++it1) {
        Partner p=*it1;
        if(!(p.has9||p.has13))continue;
        if(csvFile)fprintf(csvFile,"%s;%s;%s;%.0f;%.0f;%s;%s\n",p.call.toLocal8Bit().constData(),
                           p.canCW?"CW":"-",
                           p.locator.toLocal8Bit().constData(),
                           p.distance,
                           p.azimut,
                           p.has13?"x":"-",
                           p.has9?"x":"-");

        if(mdFile)fprintf(mdFile,"| %s | %s | %s | %.0f | %.0f | %s | %s |\n",p.call.toLocal8Bit().constData(),
                           p.canCW?"CW":"-",
                           p.locator.toLocal8Bit().constData(),
                           p.distance,
                           p.azimut,
                           p.has13?"x":"-",
                           p.has9?"x":"-");
    }
    if(csvFile)fclose(csvFile);
    if(mdFile)fclose(mdFile);


    csvFile=fopen("partnerList_ext.csv","wt");
    mdFile=fopen("partnerList_ext.md","wt");

     if(csvFile)fprintf(csvFile,"Call;CW?;Locator;Entfernung;Winkel;13?;9?\n");
     if(mdFile)fprintf(mdFile,"^ Call ^ CW?  ^  Loc ^ km^ Dir^ 13 ^9 ^\n");

     // Entfernungen und Winkel berechnen
     // Ausgabe als CSV für Excel und md für das Wiki
     for(it1=partnerList.begin(); it1!=partnerList.end(); ++it1) {
         Partner p=*it1;
         if(!(p.has9||p.has13))continue;
         if(csvFile)fprintf(csvFile,"%s;%s;%s;%.0f;%.0f;%s;%s\n",p.call.toLocal8Bit().constData(),
                            p.canCW?"CW":"-",
                            p.locator.toLocal8Bit().constData(),
                            p.distance,
                            p.azimut,
                            p.has13?"x":"-",
                            p.has9?"x":"-");

         if(mdFile)fprintf(mdFile,"| %s | %s | %s | %.0f | %.0f | %s | %s |\n",p.call.toLocal8Bit().constData(),
                            p.canCW?"CW":"-",
                            p.locator.toLocal8Bit().constData(),
                            p.distance,
                            p.azimut,
                            p.has13?"x":"-",
                            p.has9?"x":"-");
     }
     if(csvFile)fclose(csvFile);
     if(mdFile)fclose(mdFile);


     csvFile=fopen("2m.csv","wt");
     mdFile=fopen("2m.md","wt");

      if(csvFile)fprintf(csvFile,"Call;CW?;Locator;Entfernung;Winkel?\n");
      if(mdFile)fprintf(mdFile,"^ Call ^ CW?  ^  Loc ^ km^ Dir^\n");

      // Entfernungen und Winkel berechnen
      // Ausgabe als CSV für Excel und md für das Wiki
      for(it1=partnerList.begin(); it1!=partnerList.end(); ++it1) {
          Partner p=*it1;
          if(!(p.has2m))continue;
          if(csvFile)fprintf(csvFile,"%s;%s;%s;%.0f;%.0f\n",p.call.toLocal8Bit().constData(),
                             p.canCW?"CW":"-",
                             p.locator.toLocal8Bit().constData(),
                             p.distance,
                             p.azimut);

          if(mdFile)fprintf(mdFile,"| %s | %s | %s | %.0f | %.0f \n",p.call.toLocal8Bit().constData(),
                             p.canCW?"CW":"-",
                             p.locator.toLocal8Bit().constData(),
                             p.distance,
                             p.azimut);
      }
      if(csvFile)fclose(csvFile);
      if(mdFile)fclose(mdFile);


    if(dbgFile)fclose(dbgFile);
    if(reportfile)fclose(reportfile);
    //printf("Press any key to terminate the program.\n");
    //while(!kbhit());

//    return a.exec();
}

