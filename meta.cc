#include "meta.hh"
#include "support.hh"

using namespace std;

ThrottleDB::ThrottleDB() : d_sqlw("meta.sqlite3")
{
  d_sqlw.query("create table if not exists throttle (thing TEXT, reason TEXT, timestamp INT) STRICT");
  time_t now = time(0);
  d_sqlw.query("delete from throttle where timestamp < ?", {now - s_retention});
}

void ThrottleDB::report(const std::string& thing, const std::string& reason)
{
  d_sqlw.addValue({{"thing", thing}, {"reason", reason}, {"timestamp", time(nullptr)}}, "throttle");
}

bool ThrottleDB::shouldThrottle(const std::string& thing, int limitSeconds, int limit)
{
  time_t lim = time(nullptr) - limitSeconds;
  auto res = d_sqlw.queryT("select count(1) as c from throttle where thing=? and timestamp > ?",
			   {thing, lim});
  if(res.empty())
    return false;
  //    cout<<"Asked for '"<<thing<<"', limitSeconds "<<limitSeconds<<" limit "<<limit<<" lim "<<lim<<", count: "<<std::get<int64_t>(res[0]["c"])<<endl;
  
  return std::get<int64_t>(res[0]["c"]) >= limit;
}


ConversionFailureDB::ConversionFailureDB(const std::string& path) : d_sqlw(path+"meta.sqlite3")
{
  
  d_sqlw.query("CREATE TABLE if not exists ConversionFailures (\
	       id TEXT PRIMARY KEY,\
	       kind TEXT,\
               reportingTime TEXT,\
               reason TEXT,\
	       numFailures INTEGER NOT NULL DEFAULT 0,\
               attemptTime TEXT,\
               fixedHow TEXT,\
	       success INTEGER DEFAULT 0)");
}

void ConversionFailureDB::reportFailure(const std::string& id, const std::string& kind, const std::string& reason)
{
  d_sqlw.query(R"(
  INSERT INTO ConversionFailures (id, kind, reportingTime, numFailures, reason)
VALUES (?, ?, ?, 1, ?)
ON CONFLICT(id)
		 DO UPDATE SET numFailures = numFailures + 1;)", {id, kind, getDBDateTimeString(time(nullptr)), reason});

}


void ConversionFailureDB::reportFix(const std::string& id, const std::string& kind, const std::string& reason)
{
  d_sqlw.query("update ConversionFailures set fixedHow=?, attemptTime=?, success=1 where id=? and kind=?", {reason, getDBDateTimeString(time(nullptr)), id, kind});
}

void ConversionFailureDB::reportFailedAttempt(const std::string& id, const std::string& kind, const std::string& reason)
{
  d_sqlw.query("update ConversionFailures set fixedHow=?, attemptTime=?, success=0 where id=? and kind=?", {reason, getDBDateTimeString(time(nullptr)), id, kind});
}


static ConversionFailureDB::Item makeItem(const auto& f)
{
  return ConversionFailureDB::Item(
		     eget(f,"id"),
		     eget(f,"kind"),
		     eget(f,"reason"),
		     eget(f,"reportingTime"),
		     eget(f,"attemptTime"),
		     eget(f,"fixedHow"),
		     eget(f,"attemptResult"),
		     iget(f, "success")
				   );
}

vector<ConversionFailureDB::Item> ConversionFailureDB::getUnattemptedItems()
{
  auto failures = d_sqlw.queryT("select * from ConversionFailures where attemptTime is NULL");
  vector<ConversionFailureDB::Item> ret;
  for(const auto& f : failures) {
    ret.push_back(makeItem(f));
  }
  return ret;
}

vector<ConversionFailureDB::Item> ConversionFailureDB::getItems()
{
  auto failures = d_sqlw.queryT("select * from ConversionFailures");
  vector<ConversionFailureDB::Item> ret;
  for(const auto& f : failures) {
    ret.push_back(makeItem(f));
  }
  return ret;
}
