#include <QtCore>
#include "adifmodel.h"
//#include <mdebug.h>

void AdifRecord::SetEmpty(int fields)
{
        qsoNr=-1;
        selected=false;
        field.clear();
   int i;
   for(i=0;i<fields;i++)field<<QString("");
}

int AdifRecord::SetField(int dataIdx, QString data)
{
    //checken ob rec soviele Felder wie dataIdx hat, ansonsten anfügen
    if(field.count()-1<dataIdx){
        int i=field.count();
        for(;i<dataIdx+1;i++)field.append(QString(""));
    }
    field[dataIdx]=data;
    return 0;
}

AdifOptions::AdifOptions()
{
        lineLen=0;
}


AdifDataBase::AdifDataBase()
{
        useSelection=false;
        lastQSO=0;
   sendSignal=true;
   wasChanged=false;
}

void AdifDataBase::Init(QSettings *settings, QString adifTableName)
{
     QFile file(adifTableName);
     if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
         return;

     while (!file.atEnd()) {
         QString line = file.readLine().simplified();
         QString adif=line.section(QChar(' '),0,0);
         if(adif.isEmpty())continue;
         QString name=line.section(QChar(' '),1,1);
         QString descr=line.section(QChar(' '),2,-1);
         if(name.isEmpty())name=QString("[")+adif+QString("]");
         if(descr.isEmpty())descr=QString(tr("The [%1] ADIF field")).arg(adif);
         AddField(adif,name,descr);

     }
     wasChanged=false;
}



int AdifDataBase::AddQSO(AdifRecord rec)
{
    if(rec.qsoNr>0){
        int i;
        for(i=0;i<qso_s.count();i++)if(qso_s.at(i).qsoNr==rec.qsoNr){
                qso_s[i]=rec;
                break;
        }
    }else{
        rec.qsoNr=++lastQSO;
        qso_s.append(rec);
    }
    wasChanged=true;
    if(sendSignal)emit DataChanged();
    return qso_s.count()-1;
}


int AdifDataBase::GetSelectedIndex(int index)
{
    if(useSelection){
        int i,cnt=0;
        for(i=0;i<qso_s.count();i++){
                if(qso_s.at(i).selected){
                        if(index==cnt)return i;
                        cnt++;
                }
        }
        return -1;
    }else return index;

}

void AdifDataBase::SetQSOData(int index, int dataIdx, QString data)
{
    int idx=GetSelectedIndex(index);
    if(idx<0)return;
    AdifRecord rec=qso_s.at(idx);
    rec.SetField(dataIdx,data);
    qso_s[idx]=rec;
    wasChanged=true;
    if(sendSignal)emit DataChanged();
}


void AdifDataBase::DeleteQSO(int qsoNr)
{
    int index=GetIndex(qsoNr);
    if(index>=0)qso_s.removeAt(index);
    wasChanged=true;
    if(sendSignal)emit DataChanged();
}

void AdifDataBase::Select(int dataIdx,QString pattern, bool subString,bool isOr)
{
    QRegExp regExp(pattern,Qt::CaseInsensitive,QRegExp::Wildcard);
    if(subString)regExp.setPattern(QString("*")+pattern+QString("*"));
    //AdifRecord rec;
    useSelection=true;
        int i;
        for(i=0;i<qso_s.count();i++){
                QString h=qso_s.at(i).field.at(dataIdx);
                if(/*(isOr||qso_s[i].selected)&&*/regExp.exactMatch(h)){
                        qso_s[i].selected=true;
                }
        }

    if(sendSignal)emit SelectionChanged();
}

void AdifDataBase::Unselect(int dataIdx,QString pattern, bool subString)
{
    QRegExp regExp(pattern,Qt::CaseInsensitive,QRegExp::Wildcard);
    if(subString)regExp.setPattern(QString("*")+pattern+QString("*"));
    AdifRecord rec;
    useSelection=true;
        int i;
        for(i=0;i<qso_s.count();i++){
                QString h=qso_s.at(i).field.at(dataIdx);
                if(/*(isOr||qso_s[i].selected)&&*/regExp.exactMatch(h)){
                        qso_s[i].selected=false;
                }
        }
    if(sendSignal)emit SelectionChanged();
}

void AdifDataBase::Unselect(int qsoNr)
{
        int i;
        for(i=0;i<qso_s.count();i++){
                if(qso_s[i].qsoNr==qsoNr){
                        qso_s[i].selected=false;
                }
        }
    if(sendSignal)emit SelectionChanged();
}



void AdifDataBase::Select(QDateTime from, QDateTime to, bool isOr)
{
    useSelection=true;
    int dateIdx=DataIdx("QSO_DATE");
    int timeIdx=DataIdx("TIME_ON");
    if(dateIdx<0 || timeIdx<0)return;
    //AdifRecord rec;
    //foreach(rec,qso_s){
        int i;
        for(i=0;i<qso_s.count();i++){
                QString s=qso_s.at(i).field.at(dateIdx)+QString(" ")+qso_s.at(i).field.at(timeIdx).left(4);
        QDateTime date=QDateTime::fromString(s,"yyyyMMdd HHmm");
      if(/*(isOr||rec.selected) && */date<to && date>from)qso_s[i].selected=true;
    }
    if(sendSignal)emit SelectionChanged();
}

void AdifDataBase::Unselect(QDateTime from, QDateTime to)
{
    useSelection=true;
    int dateIdx=DataIdx("QSO_DATE");
    int timeIdx=DataIdx("TIME_ON");
    if(dateIdx<0 || timeIdx<0)return;
        int i;
        for(i=0;i<qso_s.count();i++){
                QString s=qso_s.at(i).field.at(dateIdx)+QString(" ")+qso_s.at(i).field.at(timeIdx).left(4);
        QDateTime date=QDateTime::fromString(s,"yyyyMMdd HHmm");
      if(/*(isOr||rec.selected) && */date<to && date>from)qso_s[i].selected=false;
    }
    if(sendSignal)emit SelectionChanged();
}

void AdifDataBase::AddSelection(int dataIdx,QString pattern, bool subString)
{
        SelectionCriteria criteria;
        if(subString)criteria.regExp.setPattern(QString("*")+pattern+QString("*"));
   else criteria.regExp.setPattern(pattern);

}

void AdifDataBase::AddSelection(QDateTime _from, QDateTime _to)
{
        from=_from;
        to=_to;
        selectionDateValid=true;
}

void AdifDataBase::Select()
{

}

void AdifDataBase::Unselect()
{

}




void AdifDataBase::ClearSelection()
{
        useSelection=false;
        AdifRecord rec;
        /*
        foreach(rec,qso_s){
                rec.selected=false;
        }
        */
        int i;
        for(i=0;i<qso_s.count();i++){
                qso_s[i].selected=false;
        }

    if(sendSignal)emit SelectionChanged();
}

bool AdifDataBase::IsSelected(int index)
{
   return qso_s.at(index).selected;
}

int AdifDataBase::GetQSOCount()
{
    return qso_s.count();
}

int AdifDataBase::GetFieldCount()
{
    return fields.count();
}

int AdifDataBase::GetSelectedCount()
{
   int cnt=0;
        AdifRecord rec;
        if(useSelection){
                foreach(rec,qso_s){
                        if(rec.selected)cnt++;
                }
           return cnt;
        }

        return 0;
}

int AdifDataBase::DataIdx(QString adifField)
{
    int i;
    QRegExp rexp(adifField,Qt::CaseInsensitive,QRegExp::FixedString);
    for(i=0;i<fields.count();i++){
        if(rexp.exactMatch(fields.at(i).adif))return i;
    }

    return AddField(adifField.toUpper(),QString("[")+adifField+QString("]"),"");
}

int AdifDataBase::AddField(QString adifName,QString fieldName,QString description)
{
      AdifField f;
      f.adif=adifName.toUpper();
      f.name=fieldName;
      f.description=description;
                fields.append(f);
                //mDebug("AddField %s %s %s",(const char*)adifName.toLocal8Bit(),
                //                (const char *)fieldName.toLocal8Bit(),
                //                (const char*)description.toLocal8Bit());
                if(sendSignal)emit AdifChanged();

        return fields.count()-1;
}


AdifRecord AdifDataBase::GetQSO(int index)
{
    int idx=GetSelectedIndex(index);
    AdifRecord rec;

    if(idx<qso_s.count()&& idx>=0)return qso_s.at(idx);
    else return rec;
}

int AdifDataBase::GetIndex(int qsoNo)
{
        int i=0;
        for(i=0;i<qso_s.count();i++){
                if(qso_s.at(i).qsoNr==qsoNo)return i;
        }
   return -1;
}


QString AdifDataBase::AdifName(int dataIdx)
{
   return fields.at(dataIdx).adif;
}

QString AdifDataBase::FieldName(int dataIdx)
{
   return fields.at(dataIdx).name;

}

QString AdifDataBase::FieldDescription(int dataIdx)
{
   return fields.at(dataIdx).description;

}

void AdifDataBase::NewDatabase()
{
        qso_s.clear();
        useSelection=false;
        wasChanged=false;
        lastQSO=0;
}

bool AdifDataBase::SetSendSignal(bool on)
{
   bool r=sendSignal;
   sendSignal=on;
   return r;
}



enum ImportState {NEW,HEADER,REJECT,FIELD,LEN,TYPE,DATA};

int AdifDataBase::ImportAdif(QString fileName, AdifOptions options, ProcessCallBack callback)
{

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return ERR_FILE_OPEN;

    QTextStream in(&file);
    AdifRecord rec;
    int qsoCnt=0;
    bool bs=SetSendSignal(false);
    QString field,data,len;
    int l=0,cnt=0;
    ImportState state=NEW;
    char text[1000];

    while (!in.atEnd()) {

        QChar c;
        in>>c;


        //char cc=c.toLatin1();
        switch(state){
        case NEW:   if(c==QChar('<'))state=FIELD;
            else state=HEADER;
            break;
        case HEADER:data+=c;
            if(c==QChar('>')){
                strncpy(text,(const char*)data.toLocal8Bit(),999);
                if(data.right(5).compare("<eoh>",Qt::CaseInsensitive)==0){
                    data="";
                    state=REJECT;
                }
            }
            break;

                case REJECT:if(c==QChar('<')){
                        state=FIELD;
                        field="";
                    }
                    break;

                case FIELD: if(c==QChar(':')){
                        state=LEN;
                        len="";
                    }
                    else if(c==QChar('>')){
                        strncpy(text,(const char*)data.toLocal8Bit(),999);
                        if(field.compare("eor",Qt::CaseInsensitive)==0){
                            rec.selected=false;
                            AddQSO(rec);
                            rec.SetEmpty(GetFieldCount());
                            state=REJECT;
                            qsoCnt++;
                            //printf(".");
                            if(callback)callback(qsoCnt);
                        }else{
                            l=0;
                            state=DATA;
                            data="";
                            cnt=0;
                        }
                    }
                    else field+=c;
                    break;

                case LEN:   if(c==QChar(':'))state=TYPE;
                    else if(c==QChar('>')){
                        strncpy(text,(const char*)field.toLocal8Bit(),999);
                        l=len.simplified().toInt();
                        if(l>0)state=DATA;
                        else state=REJECT;
                        cnt=0;
                        data="";
                    }
                    else len+=c;
                    break;

                case TYPE:  if(c==QChar('>')){
                        l=len.simplified().toInt();
                        if(l>0)state=DATA;
                        else state=REJECT;
                        cnt=0;
                        data="";
                    }
                    break;

                case DATA:  if(l&&cnt<l){
                        data+=c;
                        cnt++;
                        if(cnt>=l){
                            field=field.simplified();
                            int dataIdx=DataIdx(field);
                            if(dataIdx<0)dataIdx=AddField(field,field,"dynamically added");
                            if(dataIdx>=0)rec.SetField(dataIdx,data.simplified());
                            state=REJECT;
                        }
                    }else{
                        if(c==QChar('<')){
                            field=field.simplified();
                            int dataIdx=DataIdx(field);
                            if(dataIdx<0)dataIdx=AddField(field,field,"dynamically added");
                            if(dataIdx>=0)rec.SetField(dataIdx,data.simplified());
                            state=FIELD;
                            field="";
                        }else data+=c;
                    }
                    break;

                }//switch
    }//while

    file.close();
    //useSelection=true;
    SetSendSignal(bs);
    if(bs)PopulateChanges();
    return ERR_NO;

}


int AdifDataBase::ExportAdif(QString fileName,AdifOptions options, bool selected, ProcessCallBack callback)
{
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
         return ERR_FILE_OPEN;

        QTextStream out(&file);

        out << QString("ADIF File exported by Adifer\n%1\n<eoh>").arg(QDateTime::currentDateTime().toString("dd-MMM-yyyy HH:mm"));

        int i,qsoCnt=0;

        for(i=0;i<qso_s.count();i++){
                AdifRecord rec;
                rec=qso_s.at(i);
                if(selected && useSelection && !rec.selected)continue;
                int j,ll=0;
                for(j=0;j<fields.count();j++){
                        int len=0;
                        if(rec.field.count()>j)len=rec.field.at(j).length();
                        if(len>0){
                                QString s;
                                s=QString("<%1:%2>%3").arg(fields.at(j).adif).arg(len).arg(rec.field.at(j));
                                out<<s;
                                ll+=s.length();
                                if(options.lineLen&&ll>options.lineLen){
                                        out<<"\n";
                                        ll=0;
                                }
                        }
                }
                out << QString("<eor>\n\n");
                qsoCnt++;
                if(callback)callback(qsoCnt);

        }
   return ERR_NO;
}







