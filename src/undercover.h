#pragma once
// undercover.h — 誰是臥底 (Who's the Undercover)
#include <dpp/dpp.h>
#include <string>
#include <vector>
#include <map>
#include <random>
#include <algorithm>
#include "types.h"
#include "chips.h"
#include "ucstats.h"

// ─── Word pairs: {word_a, word_b} ────────────────────────────────────────────
static const std::vector<std::pair<std::string,std::string>> UC_PAIRS_GENERAL = {
    // 食物飲料
    {"可樂","雪碧"},{"牛奶","豆漿"},{"咖啡","奶茶"},{"火鍋","燒烤"},
    {"壽司","刺身"},{"蛋糕","麵包"},{"啤酒","紅酒"},{"漢堡","熱狗"},
    {"草莓","番茄"},{"巧克力","糖果"},{"西瓜","哈密瓜"},{"披薩","薄餅"},
    {"冰淇淋","雪糕"},{"葡萄","藍莓"},{"蘋果","梨子"},
    {"餃子","湯圓"},{"炸雞","天婦羅"},{"拉麵","烏冬"},{"饅頭","包子"},
    {"薯片","爆米花"},{"麻辣燙","串串"},{"豆腐","起司"},
    {"橙汁","蘋果汁"},{"優格","布丁"},{"紅豆湯","綠豆湯"},
    {"春捲","潤餅"},{"珍珠奶茶","霜淇淋"},{"螃蟹","龍蝦"},
    {"鮭魚","鮪魚"},{"豬肉","牛肉"},{"香蕉","芒果"},{"鳳梨","木瓜"},
    {"檸檬","萊姆"},{"鬆餅","可麗餅"},{"甜甜圈","貝果"},
    {"年糕","麻糬"},{"月餅","蛋黃酥"},{"泡麵","冬粉"},
    {"鍋貼","水餃"},{"魚丸","貢丸"},{"鳳梨酥","太陽餅"},
    {"銅鑼燒","大福"},{"布朗尼","馬芬"},{"可頌","丹麥麵包"},
    {"起司蛋糕","提拉米蘇"},{"火腿","培根"},{"三明治","潛艇堡"},
    {"壽喜燒","涮涮鍋"},{"章魚燒","炸花枝"},{"醬油","味噌"},
    {"紅茶","綠茶"},{"焦糖布丁","烤布蕾"},{"牛軋糖","太妃糖"},
    {"棉花糖","軟糖"},{"薯條","薯餅"},{"燕麥奶","米漿"},
    {"雞腿","雞翅"},{"水煮蛋","荷包蛋"},{"麻辣鍋","酸菜魚"},{"雞排","魚排"},
    // 動物
    {"貓","狗"},{"獅子","老虎"},{"鯊魚","海豚"},{"老鷹","燕子"},
    {"長頸鹿","駱駝"},{"熊貓","北極熊"},{"狼","狐狸"},{"鱷魚","蜥蜴"},
    {"蝴蝶","蜻蜓"},{"螞蟻","蜜蜂"},{"大象","犀牛"},{"孔雀","火雞"},
    {"企鵝","海豹"},{"貓頭鷹","蝙蝠"},{"寄居蟹","螃蟹"},
    {"兔子","松鼠"},{"浣熊","狸貓"},{"豬","羊"},{"雞","鴨"},
    {"鴿子","麻雀"},{"金魚","錦鯉"},{"刺蝟","豪豬"},{"水豚","海狸"},
    {"駝鳥","鴯鶓"},{"蜘蛛","蠍子"},{"青蛙","蟾蜍"},{"蛇","蚯蚓"},
    {"馬","斑馬"},{"猩猩","猴子"},{"老鼠","鼴鼠"},
    {"海星","海膽"},{"天竺鼠","倉鼠"},{"柴犬","秋田犬"},
    {"鸚鵡","八哥"},{"水母","海葵"},{"蝸牛","蛞蝓"},
    {"陸龜","海龜"},{"布偶貓","英短貓"},{"獵豹","美洲豹"},
    {"北極狐","白貂"},{"鱸魚","鯛魚"},{"河豚","比目魚"},
    {"哈士奇","馬爾濟斯"},{"藍鯨","白鯨"},{"知更鳥","夜鶯"},
    {"螢火蟲","蜉蝣"},{"帝王蟹","雪蟹"},{"羚羊","鹿"},
    {"鬣狗","野狗"},{"信天翁","海鷗"},
    // 運動
    {"足球","橄欖球"},{"籃球","排球"},{"游泳","衝浪"},
    {"棒球","壘球"},{"羽毛球","桌球"},{"網球","壁球"},
    {"跑步","競走"},{"滑板","直排輪"},{"攀岩","登山"},
    {"射箭","射擊"},{"拳擊","柔道"},{"瑜珈","皮拉提斯"},
    {"高爾夫","槌球"},{"跳水","跳傘"},{"保齡球","撞球"},
    {"飛鏢","飛盤"},{"劍道","擊劍"},{"相撲","摔角"},
    {"體操","雜技"},{"馬拉松","鐵人三項"},{"釣魚","潛水"},
    {"滑翔傘","熱氣球"},{"跆拳道","空手道"},{"划船","皮艇"},
    {"撐竿跳","跳高"},{"手球","水球"},{"圍棋","象棋"},
    {"健美","舉重"},{"越野跑","障礙賽跑"},{"花式游泳","競速游泳"},
    {"帆船","龍舟"},{"沙灘排球","沙灘足球"},{"馬術","馬球"},
    {"自行車","摩托車"},{"溜冰","花式滑冰"},{"跳繩","跳遠"},
    {"鉛球","鐵餅"},{"標槍","鏈球"},{"三人籃球","街頭籃球"},
    // 科技
    {"手機","平板"},{"耳機","喇叭"},{"電腦","筆電"},
    {"電視","投影機"},{"相機","望遠鏡"},{"遊戲機","掌機"},
    {"鍵盤","滑鼠"},{"充電器","行動電源"},{"智慧手錶","健康手環"},
    {"PS5","Xbox"},{"掃地機器人","吸塵器"},{"微波爐","烤箱"},
    {"洗碗機","洗衣機"},{"冷氣","電扇"},{"除濕機","空氣清淨機"},
    {"攝影機","監視器"},{"AR眼鏡","VR頭盔"},{"無人機","遙控飛機"},
    {"3D印表機","雷射切割機"},{"印表機","掃描機"},
    {"硬碟","隨身碟"},{"路由器","網路分享器"},
    {"藍牙耳機","有線耳機"},{"電動車","油電車"},
    {"機械鍵盤","薄膜鍵盤"},{"行車記錄器","倒車雷達"},
    {"電子白板","黑板"},{"智慧門鎖","密碼鎖"},
    {"電動牙刷","洗牙器"},{"投影機","電子看板"},
    // 地點
    {"學校","圖書館"},{"公園","廣場"},{"咖啡廳","餐廳"},
    {"電影院","劇院"},{"超市","便利商店"},{"醫院","診所"},
    {"動物園","水族館"},{"博物館","美術館"},{"火車站","機場"},
    {"健身房","游泳池"},{"教堂","廟宇"},{"夜市","市場"},
    {"夜店","KTV"},{"書店","文具店"},{"銀行","郵局"},
    {"警察局","消防局"},{"溫泉","SPA"},{"農場","牧場"},
    {"森林","叢林"},{"海灘","泳池"},{"宮殿","城堡"},
    {"大學","高中"},{"托兒所","幼稚園"},{"菜市場","魚市場"},
    {"港口","碼頭"},{"停車場","加油站"},{"理髮廳","美容院"},
    {"藥局","中藥行"},{"監獄","看守所"},{"燈塔","瞭望台"},
    {"飯店","民宿"},{"遊樂場","主題樂園"},{"溜冰場","保齡球館"},
    {"電競館","網咖"},{"天文台","天文館"},{"植物園","花市"},
    {"書法教室","美術教室"},{"古蹟","廟宇"},{"礦坑","洞穴"},
    {"戰壕","碉堡"},
    // 自然
    {"下雨","下雪"},{"太陽","月亮"},{"山","丘陵"},{"海洋","湖泊"},
    {"颱風","龍捲風"},{"彩虹","極光"},{"閃電","雷聲"},
    {"洪水","海嘯"},{"地震","火山"},{"春天","秋天"},
    {"夏天","冬天"},{"白天","夜晚"},{"清晨","黃昏"},
    {"晴天","陰天"},{"微風","暴風"},{"露水","霧氣"},
    {"冰雹","霜"},{"瀑布","噴泉"},{"珊瑚礁","海草"},
    {"沙漠","荒原"},{"河流","溪流"},{"懸崖","峭壁"},
    {"楓葉","銀杏"},{"滿月","新月"},{"北極","南極"},
    {"彗星","流星"},{"日食","月食"},{"海峽","海灣"},
    {"沼澤","濕地"},{"山洞","地洞"},
    // 日常用品
    {"眼鏡","隱形眼鏡"},{"電梯","手扶梯"},{"手錶","手環"},
    {"鉛筆","原子筆"},{"雨傘","陽傘"},{"剪刀","美工刀"},
    {"毛巾","浴巾"},{"枕頭","靠枕"},{"鑰匙","密碼鎖"},
    {"行李箱","背包"},{"牙刷","電動牙刷"},{"肥皂","洗手乳"},
    {"洗髮精","潤髮乳"},{"鏡子","玻璃"},{"蠟燭","手電筒"},
    {"日曆","手帳"},{"書籤","便利貼"},{"橡皮擦","立可白"},
    {"計算機","算盤"},{"沙漏","時鐘"},{"棉被","毛毯"},
    {"碗","盤子"},{"筷子","叉子"},{"馬克杯","水杯"},
    {"電鍋","壓力鍋"},{"鍋子","平底鍋"},{"膠帶","釘書機"},
    {"地毯","地墊"},{"窗簾","百葉窗"},{"書架","衣架"},
    {"花盆","魚缸"},{"鬧鐘","掛鐘"},{"打火機","火柴"},
    {"吸管","湯匙"},{"掃把","拖把"},{"垃圾桶","回收桶"},
    {"信封","信紙"},{"印章","簽章"},{"地圖","指南針"},
    {"望遠鏡","放大鏡"},
    // 職業
    {"醫生","護士"},{"老師","教授"},{"廚師","烘焙師"},
    {"警察","保全"},{"歌手","演員"},{"律師","法官"},
    {"消防員","救護員"},{"記者","主播"},{"設計師","插畫家"},
    {"工程師","程式設計師"},{"導遊","領隊"},{"空服員","地勤"},
    {"司機","機長"},{"藥師","中醫師"},{"外科醫師","內科醫師"},
    {"牙醫","眼科醫師"},{"心理師","社工師"},{"運動員","教練"},
    {"模特兒","網紅"},{"攝影師","導演"},{"作家","詩人"},
    {"音樂家","指揮家"},{"舞蹈家","體操選手"},{"調酒師","咖啡師"},
    {"建築師","室內設計師"},{"農夫","漁夫"},{"礦工","伐木工"},
    {"郵差","快遞員"},{"清潔工","環保員"},{"政治家","外交官"},
    // 交通
    {"公車","捷運"},{"飛機","直升機"},{"腳踏車","機車"},
    {"汽車","卡車"},{"船","潛水艇"},{"火車","高鐵"},
    {"纜車","空中廊道"},{"輪船","遊輪"},{"救護車","警車"},
    {"消防車","灑水車"},{"坦克","裝甲車"},
    {"熱氣球","滑翔翼"},{"帆船","獨木舟"},
    {"快艇","漁船"},{"計程車","共享汽車"},{"路面電車","輕軌"},
    {"越野車","吉普車"},{"電動機車","電動汽車"},{"人力車","馬車"},
    {"水上巴士","渡輪"},{"太空梭","火箭"},{"貨輪","貨機"},
    {"校車","遊覽車"},{"狗拉雪橇","雪地摩托車"},
    // 娛樂
    {"電影","電視劇"},{"漫畫","小說"},{"遊戲","桌遊"},
    {"音樂會","演唱會"},{"脫口秀","相聲"},{"動漫","卡通"},
    {"KTV","酒吧"},{"主題樂園","水上樂園"},{"馬戲團","魔術秀"},
    {"撲克牌","麻將"},{"五子棋","黑白棋"},{"直播","Podcast"},
    {"YouTube","TikTok"},{"Netflix","Disney+"},
    {"話劇","歌劇"},{"舞台劇","音樂劇"},{"魔術","特技"},
    {"射擊遊戲","格鬥遊戲"},{"RPG遊戲","策略遊戲"},
    {"恐怖遊戲","解謎遊戲"},{"偵探小說","科幻小說"},
    {"浪漫小說","武俠小說"},{"電子競技","棋牌競技"},
    {"模型","拼圖"},{"大富翁","強手棋"},{"塗色書","素描本"},
    {"填字遊戲","數獨"},{"狼人殺","誰是臥底"},{"密室逃脫","劇本殺"},
    // 服飾
    {"外套","大衣"},{"牛仔褲","休閒褲"},{"高跟鞋","平底鞋"},
    {"帽子","頭巾"},{"手套","護腕"},{"領帶","領結"},
    {"戒指","手鐲"},{"項鍊","手鍊"},{"耳環","耳釘"},
    {"圍巾","絲巾"},{"手提包","後背包"},{"墨鏡","護目鏡"},
    {"安全帽","棒球帽"},{"泳衣","比基尼"},{"運動鞋","休閒鞋"},
    // 植物
    {"玫瑰","百合"},{"向日葵","雛菊"},{"仙人掌","多肉植物"},
    {"楓樹","樺樹"},{"松樹","柏樹"},{"竹子","甘蔗"},
    {"薰衣草","迷迭香"},{"薑","大蒜"},{"紅蘿蔔","白蘿蔔"},
    {"地瓜","馬鈴薯"},{"茄子","青椒"},{"玉米","高粱"},
    {"蔥","韭菜"},{"花椰菜","白菜"},{"覆盆子","蔓越莓"},
    // 情緒
    {"開心","興奮"},{"悲傷","憂鬱"},{"憤怒","暴躁"},{"害怕","恐懼"},
    {"驚訝","震驚"},{"緊張","焦慮"},{"孤獨","寂寞"},{"滿足","幸福"},
    {"後悔","遺憾"},{"嫉妒","羨慕"},{"尷尬","羞恥"},{"厭惡","嫌棄"},
    {"疲憊","倦怠"},{"感動","感激"},{"迷茫","困惑"},{"期待","盼望"},
    {"平靜","淡然"},{"衝動","激動"},{"害羞","臉紅"},{"好奇","探索欲"},
    // 身體部位
    {"手","腳"},{"眼睛","耳朵"},{"嘴","鼻子"},{"頭髮","眉毛"},
    {"手指","腳趾"},{"肩膀","脖子"},{"胸口","腰部"},{"膝蓋","手肘"},
    {"額頭","下巴"},{"嘴唇","牙齒"},{"舌頭","喉嚨"},{"心臟","肺"},
    {"胃","腸"},{"大腦","神經"},{"骨頭","肌肉"},
    // 節慶
    {"春節","元宵節"},{"端午節","中秋節"},{"聖誕節","新年"},{"情人節","白色情人節"},
    {"萬聖節","狂歡節"},{"生日","紀念日"},{"兒童節","母親節"},{"父親節","教師節"},
    {"清明節","冬至"},{"七夕","元旦"},{"勞動節","國慶日"},{"復活節","感恩節"},
    {"花火大會","燈節"},{"音樂節","藝術節"},{"啤酒節","美食節"},{"讀書節","閱讀節"},
    {"運動會","才藝競賽"},{"盂蘭盆節","鬼節"},{"漁人節","農民節"},{"跨年","倒數"},
    // 樂器
    {"鋼琴","電子琴"},{"吉他","貝斯"},{"小提琴","中提琴"},
    {"大提琴","低音提琴"},{"笛子","簫"},{"薩克斯風","長笛"},
    {"鼓","定音鼓"},{"小號","長號"},{"手風琴","口風琴"},
    {"二胡","馬頭琴"},{"豎琴","古琴"},{"木琴","鐵琴"},
    {"班卓琴","曼陀林"},{"口琴","卡祖笛"},{"三角鐵","搖鈴"},
    {"康加鼓","邦戈鼓"},{"揚琴","古箏"},{"嗩吶","嘹笛"},{"鋼鼓","木魚"},{"鈴鼓","手搖鈴"},
    // 家具
    {"沙發","扶手椅"},{"書桌","電腦桌"},{"衣櫃","書架"},{"床","沙發床"},
    {"餐桌","茶几"},{"電視櫃","展示櫃"},{"梳妝台","化妝鏡"},{"置物架","陳列架"},
    {"鞋櫃","玄關桌"},{"嬰兒床","搖椅"},{"壁爐","暖爐"},{"酒架","餐具架"},
    {"餐椅","辦公椅"},{"掛鐘","立鐘"},{"地燈","台燈"},{"壁燈","吊燈"},
    {"浴缸","按摩浴缸"},{"馬桶","蹲廁"},{"屏風","隔板"},{"書報架","雜誌架"},
    // 美容保養
    {"口紅","唇釉"},{"眼影","眼線"},{"粉底","遮瑕"},{"腮紅","蜜粉"},
    {"睫毛膏","假睫毛"},{"指甲油","光療"},{"香水","身體乳"},{"洗面乳","卸妝水"},
    {"面膜","精華液"},{"防曬","隔離霜"},{"潤膚乳","乳液"},{"角質霜","磨砂膏"},
    {"美白霜","抗老霜"},{"眼霜","眼膜"},{"護髮素","護髮霜"},
    // 工具
    {"錘子","釘槍"},{"螺絲起子","扳手"},{"電鑽","手鑽"},{"鋸子","電鋸"},
    {"鉗子","夾子"},{"捲尺","直尺"},{"水平儀","鉛錘"},{"砂紙","打磨機"},
    {"刨刀","鑿子"},{"油漆刷","滾筒刷"},{"梯子","腳架"},
    {"焊接機","氬焊機"},{"膠槍","噴漆罐"},{"割草機","電動剪"},
    {"澆水器","噴霧瓶"},{"耕耘機","鋤頭"},{"掃雪機","吹葉機"},
    {"剪枝剪","修枝鋸"},{"水管","噴嘴"},{"安全帽","護目鏡"},
    // 個人特質
    {"外向","內向"},{"開朗","靦腆"},{"勤奮","懶惰"},{"樂觀","悲觀"},
    {"細心","粗心"},{"耐心","急性子"},{"幽默","嚴肅"},{"慷慨","吝嗇"},
    {"自信","自卑"},{"誠實","狡猾"},{"獨立","依賴"},{"謙虛","自大"},
    {"溫柔","強硬"},{"冷靜","衝動"},{"理性","感性"},
    // 學科
    {"數學","幾何"},{"物理","化學"},{"生物","生態學"},{"歷史","地理"},
    {"語文","作文"},{"音樂","藝術"},{"哲學","心理學"},{"電腦科學","資訊工程"},
    {"社會學","人類學"},{"法律","政治學"},{"經濟學","會計學"},{"天文學","地質學"},
    {"統計學","數學分析"},{"環境學","氣候學"},{"醫學","護理學"},
    // 材料
    {"木頭","竹子"},{"石頭","磚塊"},{"金屬","合金"},{"玻璃","壓克力"},
    {"皮革","人造皮"},{"棉花","麻布"},{"塑膠","矽膠"},{"橡膠","乳膠"},
    {"陶瓷","瓷器"},{"大理石","花崗岩"},{"碳纖維","玻璃纖維"},
    {"鑄鐵","鋼鐵"},{"砂","水泥"},{"樹脂","環氧樹脂"},{"石膏","石灰"},
    // 社群媒體
    {"Instagram","Facebook"},{"Twitter","微博"},{"LINE","WhatsApp"},
    {"YouTube","Bilibili"},{"Discord","Telegram"},{"TikTok","抖音"},
    {"Reddit","PTT"},{"Pinterest","Tumblr"},{"Twitch","17直播"},
    {"Snapchat","BeReal"},{"LinkedIn","Dcard"},{"Podcast","有聲書"},
    {"直播","短影片"},{"留言","私訊"},{"限時動態","貼文"},
    // 建築/結構
    {"橋","隧道"},{"摩天大樓","平房"},{"教堂","清真寺"},{"金字塔","方尖碑"},
    {"水壩","防波堤"},{"塔","煙囪"},{"倉庫","穀倉"},{"工廠","發電廠"},
    {"游泳館","體育館"},{"紀念碑","雕像"},{"公寓","別墅"},
    {"地鐵站","公車站"},{"機場","港口"},{"大學","研究所"},{"寺廟","修道院"},
    // 更多食物
    {"蔥油餅","韭菜盒"},{"刈包","潤餅"},{"臭豆腐","鹽酥雞"},{"豬血糕","黑輪"},
    {"烤玉米","烤番薯"},{"仙草","愛玉"},{"紅豆餅","車輪餅"},{"地瓜球","麻糬球"},
    {"糖葫蘆","蜜餞"},{"冬瓜茶","洛神茶"},{"荔枝","龍眼"},
    {"楊桃","蓮霧"},{"釋迦","鳳梨釋迦"},{"碗粿","米篩目"},{"肉圓","水晶肉圓"},
    {"珍珠","粉圓"},{"芋圓","地瓜圓"},{"鹹湯圓","甜湯圓"},
    {"肉羹","魷魚羹"},{"蚵仔煎","炒米粉"},{"牛肉麵","豬腳麵"},
    {"香腸炒飯","排骨炒飯"},{"雞肉飯","鴨肉飯"},{"筒仔米糕","油飯"},
    {"魯肉飯","控肉飯"},{"豬腳","滷豬蹄"},{"鴨肉","鵝肉"},
    {"涼拌黃瓜","涼拌木耳"},{"蘿蔔糕","芋頭糕"},{"榴槤","椰子"},
    {"百香果","芒果乾"},{"烏梅汁","酸梅湯"},{"薑母茶","薑汁"},
    {"蜂蜜","楓糖漿"},{"奶油","乳瑪琳"},{"起司","奶油起司"},
    {"橄欖油","葵花油"},{"醬油","魚露"},{"辣椒醬","豆瓣醬"},
    {"味噌","豆腐乳"},{"胡椒","花椒"},{"肉桂","丁香"},{"八角","茴香籽"},
    {"綠豆糕","綠豆冰"},{"紅豆湯","芋頭湯"},{"草仔粿","菜包粿"},
    {"乾麵","陽春麵"},{"燒賣","小籠包"},{"鍋貼","煎餃"},{"蒸餃","湯餃"},
    {"豆花","豆腐腦"},{"杏仁豆腐","芋圓豆花"},{"刨冰","雪花冰"},
    {"古早味蛋糕","戚風蛋糕"},{"鳳梨酥","酥餅"},{"蜂蜜蛋糕","海綿蛋糕"},
    {"布丁","烤布蕾"},{"奶酪","慕斯"},{"巧克力熔岩","巧克力布朗尼"},
    // 更多動物
    {"無尾熊","袋鼠"},{"羊駝","駱馬"},{"雪豹","雲豹"},{"貂","黃鼬"},
    {"小熊貓","大熊貓"},{"白犀牛","黑犀牛"},{"非洲象","亞洲象"},
    {"金剛鸚鵡","葵花鸚鵡"},{"巨嘴鳥","犀鳥"},{"座頭鯨","虎鯨"},
    {"大白鯊","鯨鯊"},{"神仙魚","孔雀魚"},{"金龍魚","銀龍魚"},
    {"海馬","海龍"},{"藍環章魚","箱形水母"},
    {"藍鰭鮪魚","黃鰭鮪魚"},{"鯊魚","魟魚"},
    {"竹節蟲","螳螂"},{"蟬","蟋蟀"},{"龍蝦","鰲蝦"},{"珊瑚","海葵"},
    {"海豹","海獅"},{"北極熊","棕熊"},{"座頭鯨","抹香鯨"},
    {"麋鹿","馴鹿"},{"飛鼠","鼯鼠"},{"蝙蝠","飛狐"},
    {"高山羊","岩羚羊"},{"帝王蝶","鳳蝶"},{"螢火蟲","蜻蜓"},
    {"蜣螂","甲蟲"},{"蚱蜢","蟋蟀"},{"吻仔魚","丁香魚"},
    {"海獺","水獺"},{"土撥鼠","旱獺"},{"穿山甲","食蟻獸"},
    {"火烈鳥","朱鷺"},{"白鷺","蒼鷺"},{"蜂鳥","翠鳥"},
    {"科摩多龍","巨蜥"},{"變色龍","壁虎"},{"眼鏡蛇","蟒蛇"},
    // 更多地點
    {"百貨公司","購物中心"},{"展覽館","會議中心"},{"文創園區","設計中心"},
    {"天台","露台"},{"倉庫","儲藏室"},{"自助洗衣店","乾洗店"},
    {"玩具店","童裝店"},{"電器行","手機店"},{"社區中心","活動中心"},
    {"博覽會","展銷會"},{"花市","植物園"},{"古蹟","文化遺址"},
    {"禪寺","道觀"},{"驛站","客棧"},{"燈塔","瞭望塔"},
    // 更多娛樂
    {"沙盒遊戲","開放世界遊戲"},{"單機遊戲","多人遊戲"},{"手遊","主機遊戲"},
    {"電影原聲帶","電影配樂"},{"歌舞劇","音樂劇"},{"解謎遊戲","逃脫遊戲"},
    {"武打電影","動作電影"},{"紀錄片","真人秀"},{"競賽節目","益智節目"},{"動作漫畫","少女漫畫"},
    // 更多服飾
    {"洋裝","連身褲"},{"西裝","晚禮服"},{"旗袍","和服"},{"毛衣","針織衫"},
    {"連帽衣","衛衣"},{"運動裝","瑜珈褲"},{"短褲","熱褲"},
    {"防風外套","防水外套"},{"圍裙","工作服"},{"泳帽","浴帽"},
    {"兔耳髮夾","蝴蝶結髮夾"},{"側背包","斜背包"},{"行李腰包","小錢包"},
    {"遮陽帽","鴨舌帽"},{"棒球帽","漁夫帽"},
    // 更多植物
    {"橡樹","楓樹"},{"杉木","紅木"},{"銀杏","梧桐"},{"睡蓮","荷花"},
    {"蘭花","茉莉"},{"梅花","桃花"},{"菊花","大波斯菊"},{"薄荷","羅勒"},
    {"迷迭香","百里香"},{"蕨類","棕櫚"},{"仙人柱","龍舌蘭"},{"捕蠅草","豬籠草"},
    {"水仙","鬱金香"},{"康乃馨","洋桔梗"},{"海芋","天堂鳥"},
    {"紫藤","爬牆虎"},{"常春藤","蔓性玫瑰"},{"苦楝","木棉"},{"台灣欒樹","鳳凰木"},{"苔蘚","地衣"},
    // 科技補充
    {"人工智慧","機器學習"},{"區塊鏈","加密貨幣"},{"虛擬實境","擴增實境"},
    {"量子電腦","超級電腦"},{"基因編輯","基因定序"},{"奈米科技","微機電"},
    {"太陽能板","風力發電機"},{"電動車","混合動力車"},{"衛星","太空站"},{"光纖","5G"},
    // 運動補充
    {"滑雪","單板滑雪"},{"衝浪","風帆衝浪"},{"自由車","登山車"},
    {"三鐵","鐵人三項"},{"馬拉松","超級馬拉松"},{"攀岩","抱石"},
    {"武術","功夫"},{"水球","水中橄欖球"},{"藤球","板球"},{"壘球","棒球"},
    // 雜項
    {"彩票","刮刮樂"},{"撲克牌","塔羅牌"},{"骰子","硬幣"},{"陀螺","悠悠球"},
    {"密碼","指紋"},{"鑰匙","磁卡"},{"合約","協議"},{"保險","保障"},
    {"股票","基金"},{"期貨","外匯"},{"房貸","車貸"},{"存款","定存"},
    {"信用卡","簽帳卡"},{"電子錢包","行動支付"},{"發票","收據"},{"報稅","退稅"},
    {"法院","仲裁所"},{"獎學金","助學金"},{"實習","兼職"},{"創業","副業"},
    {"投資","理財"},{"預算","財務計畫"},{"加薪","升職"},{"論文","報告"},
    {"實驗室","研究室"},{"學位","學歷"},{"補習班","家教"},{"社團","俱樂部"},
};

// ─── 動漫題庫（作品名稱）────────────────────────────────────────────────────────
static const std::vector<std::pair<std::string,std::string>> UC_PAIRS_ANIME = {
    // 少年熱血
    {"火影忍者","海賊王"},{"鬼滅之刃","咒術迴戰"},{"七龍珠","聖鬥士星矢"},
    {"進擊的巨人","鏈鋸人"},{"我的英雄學院","一拳超人"},
    {"鋼之鍊金術師","獵人"},{"銀魂","亂馬½"},
    {"犬夜叉","幽遊白書"},{"浪客劍心","烏龍派出所"},
    {"死亡筆記本","心理測量者"},{"約定的夢幻島","寄生獸"},
    {"葬送的芙莉蓮","黑色四葉草"},{"Spy×Family","驚爆危機"},
    {"全職獵人","妖精的尾巴"},{"黑色五葉草","我的英雄學院"},
    {"鬼滅之刃","進擊的巨人"},{"咒術迴戰","我的英雄學院"},
    // 體育
    {"黑子的籃球","灌籃高手"},{"足球小將","藍色監獄"},
    {"棋靈王","網球王子"},{"排球少年","彈丸論破"},
    {"弱蟲踏板","飆速宅男"},{"鑽石王牌","棒球英豪"},
    // 機器人/科幻
    {"新世紀福音戰士","機動戰士鋼彈"},{"天元突破格雷古拉","超時空要塞"},
    {"鋼彈SEED","鋼彈00"},{"魔神Z","蓋特機器人"},
    // 異世界
    {"刀劍神域","無職轉生"},{"Re:Zero","關於我轉生變成史萊姆這檔事"},
    {"異世界居酒屋阿信","在異世界迷宮開後宮"},
    {"盾之勇者成名錄","蜘蛛啊，然後呢"},
    {"龍與雀斑公主","Belle"},
    // 吉卜力
    {"龍貓","千與千尋"},{"天空之城","幽靈公主"},{"魔女宅急便","霍爾的移動城堡"},
    {"側耳傾聽","心之谷"},{"紅豬","幽靈公主"},
    {"借東西的小人阿莉埃蒂","貓的報恩"},{"螢火蟲之墓","歡迎光臨奇異小鎮"},
    {"魔法公主","崖上的波妞"},{"起風了","你想活出怎樣的人生"},
    // 新海誠
    {"你的名字","天氣之子"},{"鈴芽之旅","言葉之庭"},
    {"秒速五公分","追逐繁星的孩子"},{"雲之彼端，約定的地方","星之聲"},
    // 戀愛/青春
    {"四月是你的謊言","月刊少女野崎君"},{"我的青春戀愛物語","青春豬頭少年"},
    {"水果籃子","娜娜"},{"聲之形","未聞花名"},
    {"我們仍未知道那天所見的花名","AnoHana"},
    {"冰菓","日常"},{"玉子市場","小林家的龍女僕"},
    // 日常/療癒
    {"涼宮春日的憂鬱","K-ON！輕音部"},{"孤獨搖滾","輕音少女"},
    {"搖曳露營","悠哉日常大王"},{"工作細胞","工作細胞BLACK"},
    {"Spy×Family","不時輕搖的少女"},
    // 推理/懸疑
    {"名偵探柯南","金田一少年之事件簿"},{"命運石之門","無頭騎士異聞錄"},
    {"魔法少女小圓","魔法少女育成計畫"},{"約定的夢幻島","末日時在做什麼"},
    // 兒童/經典
    {"哆啦A夢","蠟筆小新"},{"神奇寶貝","數碼寶貝"},{"遊戲王","數碼寶貝"},
    {"魔法少女小圓","美少女戰士"},{"魔法陣少女","卡片美少女"},
    {"鐵臂阿童木","無敵鐵金剛"},{"科學小飛俠","超人力霸王"},
    {"哆啦A夢","科學小飛俠"},{"神奇寶貝","數碼寶貝"},
    // 奇幻/冒險
    {"物語系列","人形電腦天使心"},{"魔法騎士","Card Captor Sakura"},
    {"瓦塔那貝","輝夜姬想讓人告白"},{"輝夜姬想讓人告白","我推的孩子"},
    {"葬送的芙莉蓮","迷宮飯"},{"盜賊家族","迷宮飯"},
};

// ─── 歌曲題庫（華語＋日語） ───────────────────────────────────────────────────
static const std::vector<std::pair<std::string,std::string>> UC_PAIRS_SONGS = {
    // ═══ 華語 ═══
    // 周杰倫
    {"稻香","七里香"},{"告白氣球","給我一首歌的時間"},{"等你下課","不能說的秘密"},
    {"夜曲","安靜"},{"晴天","龍捲風"},{"以父之名","天台"},
    {"愛在西元前","印第安老斑鳩"},{"我不配","擱淺"},{"簡單愛","可愛女人"},
    {"菊花台","青花瓷"},{"雙節棍","蝸牛"},{"軌跡","園遊會"},
    {"煙花易冷","珊瑚海"},{"本草綱目","霍元甲"},{"驚歎號","超人不會飛"},
    {"你好嗎","手寫的從前"},{"迷迭香","說好不哭"},{"周杰倫","林俊傑"},
    {"周杰倫","陶喆"},{"周杰倫","王力宏"},
    // 林俊傑
    {"江南","一千年以後"},{"你比從前快樂","不為誰而作的歌"},
    {"修煉愛情","可惜沒如果"},{"距離","背對背擁抱"},
    {"醉赤壁","醉騎士"},{"交換余生","不就這樣喜歡你"},
    {"她說","關鍵詞"},{"進階","偉大的渺小"},
    // 五月天
    {"諾亞方舟","乾杯"},{"知足","任意門"},{"溫柔","這輩子"},
    {"倔強","突然好想你"},{"我心中尚未崩壞的地方","後來的我們"},
    {"你是我的花朵","好好"},{"憨人","一顆蘋果"},
    {"頑固","洗腦"},{"成名在望","離開地球表面"},
    // 蔡依林
    {"玫瑰少年","UGLY BEAUTY"},{"說愛你","我呢"},{"倒帶","迷幻"},
    {"日不落","布拉格廣場"},{"舞孃","特務J"},{"消極掰","惡女"},
    {"紅衣女孩","嗆辣"},{"大藝術家","野蠻遊戲"},
    // 張惠妹
    {"聽海","原來你什麼都不要"},{"我恨我愛你","Bad Boy"},
    {"我可以抱你嗎","剪愛"},{"記得","勇敢"},
    {"Bad Boy","你是愛我的"},{"三天三夜","姐妹"},
    // 鄧紫棋
    {"泡沫","光年之外"},{"喜歡你","多遠都要在一起"},
    {"新的心跳","句號"},{"漫無目的","收藏"},
    // 歌手比較
    {"周杰倫","蔡依林"},{"周杰倫","孫燕姿"},{"林俊傑","蕭亞軒"},
    {"五月天","蘇打綠"},{"五月天","盧廣仲"},{"八三夭","草東沒有派對"},
    {"鄧紫棋","曲婉婷"},{"家家","郭靜"},{"張惠妹","張韶涵"},
    {"方大同","陳奕迅"},{"李榮浩","周杰倫"},
    // 陳奕迅
    {"富士山下","好久不見"},{"十年","愛情轉移"},{"浮誇","人來人往"},
    {"最佳損友","葡萄成熟時"},{"陳奕迅","周杰倫"},
    // 台語歌
    {"愛拚才會贏","家後"},{"夜市人生","流浪到淡水"},
    {"望春風","補破網"},{"雨夜花","心酸酸"},
    {"向前走","恰恰"},{"感謝","孤單北半球"},
    {"阿母","阿爸"},{"思慕的人","河邊春夢"},
    // 鄧麗君
    {"月亮代表我的心","甜蜜蜜"},{"再見我的愛人","你怎麼說"},
    {"夜來香","我只在乎你"},{"小城故事","路邊的野花不要採"},
    {"何日君再來","夜上海"},
    // 香港粵語/經典
    {"張國榮 - Monica","張國榮 - 風繼續吹"},
    {"張學友 - 吻別","張學友 - 一路上有你"},
    {"劉德華 - 冰雨","劉德華 - 17歲"},
    {"梅艷芳 - 壞女孩","梅艷芳 - 夕陽之歌"},
    {"王菲 - 紅豆","王菲 - 夢中人"},
    {"譚詠麟 - 遲來的春天","譚詠麟 - 愛情陷阱"},
    // 台灣老歌
    {"羅大佑 - 童年","羅大佑 - 光陰的故事"},
    {"蔡琴 - 被遺忘的時光","鳳飛飛 - 跟著感覺走"},
    {"齊秦 - 狼","齊秦 - 外面的世界"},
    {"庾澄慶 - 情非得已","庾澄慶 - 讓我一次愛個夠"},
    {"張清芳 - 其實你不懂我的心","黃品源 - 你怎麼捨得我難過"},
    // 音樂類型
    {"台語流行","國語流行"},{"抒情","快節奏"},
    {"男聲","女聲"},{"合唱","獨唱"},{"現場演唱","錄音室版"},
    {"演唱會","直播"},{"翻唱","原曲"},
};


// ─── 成人題庫（18+） ─────────────────────────────────────────────────────────
static const std::vector<std::pair<std::string,std::string>> UC_PAIRS_ADULT = {
    // 身體部位
    {"胸部","臀部"},{"乳溝","臀溝"},{"大奶","翹臀"},{"巨乳","豐臀"},
    {"胸罩","內褲"},{"大腿","小腿"},{"屁股","乳房"},{"脖子","鎖骨"},
    {"大腿內側","腋下"},{"腹肌","馬甲線"},{"刺青","穿環"},
    {"鎖骨","腰線"},{"腰身","臀部曲線"},
    // 情趣用品
    {"震動棒","跳蛋"},{"情趣用品","按摩棒"},{"保險套","避孕藥"},
    {"潤滑液","情趣噴劑"},{"手銬","眼罩"},{"皮鞭","蠟燭"},
    {"充氣娃娃","矽膠娃娃"},{"情趣內衣","透視裝"},{"SM道具","捆綁繩"},
    // 性行為
    {"做愛","打砲"},{"前戲","正式上陣"},{"口交","手交"},
    {"自慰","手淫"},{"高潮","潮吹"},{"射精","噴射"},
    {"舌吻","法式親吻"},{"親嘴","接吻"},{"調情","挑逗"},
    {"強力","溫柔"},{"快速","緩慢"},{"一夜情","炮友"},
    {"體外射精","內射"},{"無套","包套"},{"側入","後入"},
    {"女上位","男上位"},{"站立式","騎乘式"},
    // 成人媒體
    {"A片","色情小說"},{"裸照","性愛影片"},{"春宮圖","春宮片"},
    {"情色","色情"},{"男優","女優"},{"老師系","護士系"},
    {"熟女系","蘿莉系"},{"制服系","素人系"},{"實況主","成人主播"},
    {"自拍","偷拍"},{"OnlyFans","情色直播"},
    // 性感外貌
    {"性感","可愛"},{"蘿莉","正妹"},
    {"比基尼","三點式"},{"透視裝","半透明衣"},
    {"絲襪","大腿絲襪"},{"丁字褲","無痕內褲"},
    {"裸體","半裸"},{"泳衣","情趣服"},{"肉感","纖細"},
    {"兔女郎","女僕裝"},
    {"制服誘惑","空姐裝"},{"護士服","學生服"},
    // 關係型態
    {"外遇","小三"},{"砲友","性伴侶"},
    {"開放性關係","交換伴侶"},{"援交","應召"},
    {"嫖娼","召妓"},{"劈腿","腳踏兩條船"},
    {"炮友","朋友"},{"養小狼狗","包養"},
    // 情慾感受
    {"慾望","衝動"},{"飢渴","渴望"},{"刺激","快感"},
    {"興奮","燥熱"},{"撩撥","挑逗"},{"春心","情慾"},
    {"壓抑","放縱"},{"害羞","大膽"},{"羞恥","快感"},
    // 場景
    {"汽車旅館","鐘點房"},{"野外","公廁"},{"更衣室","廁所"},
    {"床","沙發"},{"浴室","泳池"},{"陽台","車內"},
    {"偷情","外遇"},{"一夜情","短暫激情"},
    // 生理/知識
    {"上床","睡在一起"},{"排卵期","安全期"},
    {"勃起","硬起來"},{"射精","精子"},{"高潮","G點"},
    // 重口味體位
    {"狗爬式","側臥式"},{"四腳獸","正常體位"},
    {"深喉嚨","口爆"},{"顏射","內射"},{"肛交","後庭"},
    {"雙管齊下","夾三明治"},{"多P","換妻"},
    {"拳交","腳交"},{"噴水","潮吹"},{"吞精","吐精"},
    // 重口味器官
    {"陰莖","陰道"},{"龜頭","陰唇"},{"睪丸","卵巢"},
    {"包皮","陰蒂"},{"乳頭","乳暈"},{"陰毛","腋毛"},
    {"肛門","會陰"},{"精液","愛液"},{"陰囊","子宮"},
    // SM / 特殊癖好
    {"SM","角色扮演"},{"主人","奴隸"},{"施虐","受虐"},
    {"踩踏","舔腳"},{"蠟燭滴","冰塊刺激"},{"窒息感","被束縛感"},
    {"羞辱","讚美"},{"懲罰","獎勵"},{"調教","訓練"},
    {"戴項圈","戴手銬"},{"跪地","趴下"},
    // 特殊場景/情境
    {"強暴幻想","合意暴力"},{"偷窺","被偷窺"},
    {"公共場所","密室"},{"直播做愛","拍片"},
    {"老師學生","醫生病人"},{"老闆秘書","警察嫌犯"},
    {"繼母繼子","叔叔姪女"},{"鄰居勾引","快遞上門"},
    // 台灣/日系用語
    {"屌","雞巴"},{"奶","咪咪"},
    {"開苞","破處"},{"處女","處男"},
    {"淫叫","嬌喘"},{"發情","飢渴"},{"淫蕩","騷"},
    {"按摩棒震動","跳蛋遙控"},{"假屌","假陽具"},
    // 更多行為/動作
    {"插入","抽插"},{"磨蹭","頂入"},{"深插","淺插"},
    {"快插","慢插"},{"連續抽插","一插到底"},
    {"摸胸","揉胸"},{"捏奶","吸奶"},{"舔乳頭","咬乳頭"},
    {"摸下體","插手指"},{"舔陰","舔肛"},
    {"打飛機","口交"},{"擼管","自慰"},{"打手槍","打炮"},
    {"後入式","正常位"},{"騎乘位","站立後入"},
    {"夾胸位","腿放肩上"},{"蓮花座","湯匙式"},
    // 更多感覺描述
    {"濕潤","硬挺"},{"緊緊的","鬆鬆的"},
    {"敏感","遲鈍"},{"抽搐","痙攣"},{"顫抖","僵硬"},
    // 更多器具
    {"肛塞","後庭塞"},{"開陰器","窺陰器"},
    {"乳夾","乳環"},{"陰環","龜頭環"},
    {"電動假屌","旋轉假屌"},{"飛機杯","口交杯"},
    {"電動床","鏡子房"},
    // 更多癖好/fetish
    {"足交","腋交"},{"乳交","大腿夾"},
    {"網襪","黑絲"},{"皮革","乳膠"},
    {"尿玩","排泄癖"},{"吐口水","吞口水"},
    {"性愛鞦韆","性愛椅"},
    // 更多情境
    {"偷拍廁所","偷拍更衣室"},
    {"輪流上","排隊操"},{"公開羞辱","當眾脫衣"},
    {"一夜五次","做到天亮"},{"邊做邊直播","邊做邊拍"},
    {"素人自拍","第一次拍片"},
};

// ─── Pool selector ──────────────────────────────────────────────────────────
static const std::vector<std::pair<std::string,std::string>>& get_uc_pool(const std::string& key) {
    if (key == "anime")  return UC_PAIRS_ANIME;
    if (key == "songs")  return UC_PAIRS_SONGS;
    if (key == "adult")  return UC_PAIRS_ADULT;
    return UC_PAIRS_GENERAL;
}

// ─── Forward declarations ─────────────────────────────────────────────────────
static void uc_do_eliminate_confirmed(uint64_t gid, dpp::snowflake elim_uid);
static void uc_end_game(uint64_t gid, bool civ_win, bool by_guess = false);
static void uc_start_round(uint64_t gid);

// ─── Helpers ──────────────────────────────────────────────────────────────────

// Count non-civilians (undercover or blank) who are alive
static int uc_spy_alive(const UCGame& g) {
    int n = 0;
    for (const auto& p : g.players)
        if (p.alive && (p.is_undercover || p.is_blank)) n++;
    return n;
}
static int uc_civ_alive(const UCGame& g) {
    int n = 0;
    for (const auto& p : g.players)
        if (p.alive && !p.is_undercover && !p.is_blank) n++;
    return n;
}
static bool uc_civs_win(const UCGame& g) { return uc_spy_alive(g) == 0; }
static bool uc_spy_wins(const UCGame& g)  { return uc_spy_alive(g) >= uc_civ_alive(g); }

static int uc_spy_num(int n) {
    if (n >= 12) return 3;
    if (n >= 8)  return 2;
    return 1;
}

static UCPlayer* uc_find(UCGame& g, dpp::snowflake uid) {
    for (auto& p : g.players) if (p.uid == uid) return &p;
    return nullptr;
}

// ─── Lobby message ────────────────────────────────────────────────────────────

static dpp::message uc_lobby_msg(const UCGame& g) {
    dpp::embed e;
    e.set_title("🕵️ 誰是臥底 — 等待玩家").set_color(0x9B59B6);

    int n = (int)g.players.size();
    std::string mode_str = g.blank_mode ? "🃏 白板模式" : "🕵️ 臥底模式";
    std::string pool_str =
        (g.word_pool == "anime") ? "🎌 動漫" :
        (g.word_pool == "songs") ? "🎵 歌曲" :
        (g.word_pool == "adult") ? "🔞 成人內容" : "🎯 一般";
    std::string desc = "主持人：<@" + std::to_string((uint64_t)g.host_id) + ">\n";
    desc += "目前模式：**" + mode_str + "**　題庫：**" + pool_str + "**\n\n";

    desc += "**規則：**\n";
    if (g.blank_mode) {
        desc += "• 所有人拿到相同的詞，白板玩家沒有詞，需自行判斷\n";
        desc += "• 投票淘汰白板 → 白板出局則平民勝；白板存活至人數 ≥ 平民則白板勝\n";
    } else {
        desc += "• 平民和臥底各拿到不同但相近的詞，輪流描述、不能直接說出\n";
        desc += "• 投票淘汰臥底 → 全部臥底出局則平民勝；臥底數 ≥ 平民數則臥底勝\n";
    }
    desc += "• 被淘汰者有一次機會猜出平民詞，猜對可翻盤\n";
    desc += "• 勝利：+50 碼　落敗：+20 碼\n\n";

    std::string spy_label = g.blank_mode
        ? "白板 1 人"
        : "臥底 " + std::to_string(uc_spy_num(n)) + " 人";
    desc += "**玩家 (" + std::to_string(n) + "/12，至少4人，" + spy_label + ")：**\n";
    for (size_t i = 0; i < g.players.size(); i++)
        desc += std::to_string(i+1) + ". " + g.players[i].display_name + "\n";
    if (g.players.empty()) desc += "（尚無玩家）\n";

    e.set_description(desc);
    e.set_footer(dpp::embed_footer().set_text("遊戲 #" + std::to_string(g.id)));

    std::string gs = std::to_string(g.id);
    dpp::component r1, r_pool, r2;
    r1.set_type(dpp::cot_action_row);
    r_pool.set_type(dpp::cot_action_row);
    r2.set_type(dpp::cot_action_row);

    r1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🙋 加入").set_id("uc_join_" + gs).set_style(dpp::cos_success));
    r1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🚪 離開").set_id("uc_leave_" + gs).set_style(dpp::cos_secondary));
    r1.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label(g.blank_mode ? "切換：🕵️ 臥底模式" : "切換：🃏 白板模式")
        .set_id("uc_mode_" + gs)
        .set_style(dpp::cos_secondary));

    // 題庫選擇下拉選單
    dpp::component pool_sel;
    pool_sel.set_type(dpp::cot_selectmenu)
        .set_id("uc_pool_" + gs)
        .set_placeholder("📚 選擇題庫");
    pool_sel.add_select_option(dpp::select_option("🎯 一般題庫", "general", "涵蓋生活、食物、動物、科技等各類主題")
        .set_default(g.word_pool == "general"));
    pool_sel.add_select_option(dpp::select_option("🎌 動漫", "anime", "知名動漫角色、作品、招式比較")
        .set_default(g.word_pool == "anime"));
    pool_sel.add_select_option(dpp::select_option("🎵 歌曲", "songs", "台灣、韓國、日本、西洋知名歌曲與歌手")
        .set_default(g.word_pool == "songs"));
    r_pool.add_component(pool_sel);

    r2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("▶️ 開始遊戲").set_id("uc_start_" + gs)
        .set_style(dpp::cos_primary).set_disabled(n < 4));
    r2.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🗑️ 解散").set_id("uc_dissolve_" + gs).set_style(dpp::cos_danger));

    return dpp::message().add_embed(e).add_component(r1).add_component(r_pool).add_component(r2);
}

// ─── Describe phase message ───────────────────────────────────────────────────

static dpp::message uc_describe_msg(const UCGame& g) {
    dpp::embed e;
    e.set_title("🕵️ 誰是臥底 — 第" + std::to_string(g.round) + "輪發言").set_color(0x3498DB);

    bool all_done = (g.speak_pos >= (int)g.speak_order.size());
    std::string cur_name;
    if (!all_done) {
        for (auto& p : g.players)
            if (p.uid == g.speak_order[g.speak_pos]) { cur_name = p.display_name; break; }
    }

    std::string desc;
    for (int i = 0; i < (int)g.speak_order.size(); i++) {
        dpp::snowflake u = g.speak_order[i];
        std::string nm;
        for (auto& p : g.players) if (p.uid == u) { nm = p.display_name; break; }
        if (i < g.speak_pos) {
            auto ait = g.answers.find(u);
            std::string ans = (ait != g.answers.end() && !ait->second.empty())
                ? "「" + ait->second + "」" : "（跳過）";
            desc += "✅ **" + nm + "**：" + ans + "\n";
        } else if (i == g.speak_pos) {
            desc += "🎤 **" + nm + "** — 發言中...\n";
        } else {
            desc += "⏳ " + nm + " — 等待中\n";
        }
    }

    if (all_done) {
        desc += "\n✅ **所有玩家已發言完畢，即將進入投票...**";
        e.set_description(desc);
        return dpp::message().add_embed(e);
    }

    desc += "\n📢 **" + cur_name + "**：請點擊下方按鈕輸入你的描述！";
    e.set_description(desc);
    e.set_footer(dpp::embed_footer().set_text(
        "遊戲 #" + std::to_string(g.id) + "　主持人可點擊跳過"));

    std::string gs = std::to_string(g.id);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("💬 輸入回答").set_id("uc_answer_" + gs)
        .set_style(dpp::cos_success));
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("⏭️ 跳過（主持人）").set_id("uc_spoke_" + gs)
        .set_style(dpp::cos_secondary));

    return dpp::message().add_embed(e).add_component(row);
}

// ─── All-answers summary (posted after all players have spoken) ───────────────

static dpp::message uc_all_answers_msg(const UCGame& g) {
    dpp::embed e;
    e.set_title("💬 第" + std::to_string(g.round) + "輪發言結果").set_color(0x5865F2);
    std::string desc;
    for (auto uid : g.speak_order) {
        std::string nm;
        for (auto& p : g.players) if (p.uid == uid) { nm = p.display_name; break; }
        auto it = g.answers.find(uid);
        std::string ans = (it != g.answers.end() && !it->second.empty())
            ? "「" + it->second + "」" : "（已跳過）";
        desc += "**" + nm + "**：" + ans + "\n\n";
    }
    e.set_description(desc.empty() ? "（無發言紀錄）" : desc);
    e.set_footer(dpp::embed_footer().set_text(
        "遊戲 #" + std::to_string(g.id) + "　即將開始投票..."));
    return dpp::message().add_embed(e);
}

// ─── Vote message ─────────────────────────────────────────────────────────────

static dpp::message uc_vote_msg(const UCGame& g) {
    std::map<dpp::snowflake, int> tally;
    for (auto& p : g.players) if (p.alive) tally[p.uid] = 0;
    for (auto& [vt, tg] : g.votes)
        if (tg && tally.count(tg)) tally[tg]++;

    int alive = (int)tally.size();
    int voted = (int)g.votes.size();

    dpp::embed e;
    e.set_title("🗳️ 誰是臥底 — 第" + std::to_string(g.round) + "輪投票").set_color(0xE67E22);

    std::string desc = "**已投票 " + std::to_string(voted) + "/" + std::to_string(alive) + " 人**\n\n";
    for (auto& p : g.players) {
        if (!p.alive) continue;
        desc += "• " + p.display_name + " — **" + std::to_string(tally[p.uid]) + "票**";
        if (g.votes.count(p.uid)) {
            auto tgt = g.votes.at(p.uid);
            if (tgt) {
                std::string tn;
                for (auto& p2 : g.players) if (p2.uid == tgt) { tn = p2.display_name; break; }
                desc += "（→ " + tn + "）";
            } else {
                desc += "（→ 棄票）";
            }
        }
        desc += "\n";
    }

    e.set_description(desc);
    e.set_footer(dpp::embed_footer().set_text(
        "遊戲 #" + std::to_string(g.id) + "　主持人可強制結算"));

    dpp::message msg; msg.add_embed(e);
    std::string gs = std::to_string(g.id);

    std::vector<dpp::snowflake> alive_uids;
    for (auto& p : g.players) if (p.alive) alive_uids.push_back(p.uid);

    dpp::component row; row.set_type(dpp::cot_action_row);
    int cnt = 0;
    for (auto u : alive_uids) {
        std::string nm;
        for (auto& p : g.players) if (p.uid == u) { nm = p.display_name; break; }
        std::string label = nm + "（" + std::to_string(tally[u]) + "票）";
        if (label.size() > 80) label = label.substr(0, 77) + "...";
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(label)
            .set_id("uc_vote_" + gs + "_" + std::to_string((uint64_t)u))
            .set_style(dpp::cos_secondary));
        cnt++;
        if (cnt == 5) {
            msg.add_component(row);
            row = dpp::component(); row.set_type(dpp::cot_action_row);
            cnt = 0;
        }
    }
    if (cnt > 0) msg.add_component(row);

    dpp::component ctrl; ctrl.set_type(dpp::cot_action_row);
    ctrl.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🚫 棄票").set_id("uc_vskip_" + gs).set_style(dpp::cos_secondary));
    ctrl.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("⚡ 強制結算").set_id("uc_vforce_" + gs).set_style(dpp::cos_danger));
    msg.add_component(ctrl);

    return msg;
}

// ─── PK vote message ──────────────────────────────────────────────────────────

static dpp::message uc_pk_msg(const UCGame& g) {
    std::map<dpp::snowflake, int> tally;
    for (auto c : g.pk_candidates) tally[c] = 0;
    for (auto& [vt, cd] : g.pk_votes)
        if (tally.count(cd)) tally[cd]++;

    int eligible = 0;
    for (auto& p : g.players) {
        if (!p.alive) continue;
        bool is_cand = std::find(g.pk_candidates.begin(), g.pk_candidates.end(), p.uid)
                       != g.pk_candidates.end();
        if (!is_cand) eligible++;
    }
    int voted = (int)g.pk_votes.size();

    dpp::embed e;
    e.set_title("⚡ PK 加時！票數相同").set_color(0xE74C3C);

    std::string desc = "以下玩家票數相同，進入 PK 再投票！\n\n**PK 玩家：**\n";
    for (auto c : g.pk_candidates) {
        std::string nm;
        for (auto& p : g.players) if (p.uid == c) { nm = p.display_name; break; }
        desc += "• " + nm + " — **" + std::to_string(tally[c]) + "票**\n";
    }
    desc += "\n**已投票 " + std::to_string(voted) + "/" + std::to_string(eligible) + " 人**\n";

    for (auto& p : g.players) {
        if (!p.alive) continue;
        bool is_cand = std::find(g.pk_candidates.begin(), g.pk_candidates.end(), p.uid)
                       != g.pk_candidates.end();
        if (is_cand || !g.pk_votes.count(p.uid)) continue;
        auto tgt = g.pk_votes.at(p.uid);
        std::string tn;
        for (auto& p2 : g.players) if (p2.uid == tgt) { tn = p2.display_name; break; }
        desc += p.display_name + " → " + tn + "\n";
    }

    e.set_description(desc);
    e.set_footer(dpp::embed_footer().set_text(
        "遊戲 #" + std::to_string(g.id) + "　PK 玩家不能投票"));

    dpp::message msg; msg.add_embed(e);
    std::string gs = std::to_string(g.id);

    dpp::component row; row.set_type(dpp::cot_action_row);
    for (auto c : g.pk_candidates) {
        std::string nm;
        for (auto& p : g.players) if (p.uid == c) { nm = p.display_name; break; }
        std::string label = nm + "（" + std::to_string(tally[c]) + "票）";
        row.add_component(dpp::component().set_type(dpp::cot_button)
            .set_label(label)
            .set_id("uc_pk_" + gs + "_" + std::to_string((uint64_t)c))
            .set_style(dpp::cos_danger));
    }
    msg.add_component(row);

    dpp::component ctrl; ctrl.set_type(dpp::cot_action_row);
    ctrl.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("⚡ 強制結算").set_id("uc_pkforce_" + gs).set_style(dpp::cos_secondary));
    msg.add_component(ctrl);

    return msg;
}

// ─── Elimination announcement (no identity reveal) ───────────────────────────

static dpp::message uc_elim_msg(const UCGame& g, dpp::snowflake elim_uid) {
    std::string nm;
    for (auto& p : g.players) if (p.uid == elim_uid) { nm = p.display_name; break; }

    dpp::embed e;
    e.set_title("🗳️ 本輪淘汰").set_color(0x95A5A6);
    e.set_description("**" + nm + "** 被淘汰！");

    std::string alive_str;
    for (auto& p : g.players) {
        if (!p.alive) continue;
        alive_str += "• " + p.display_name + "\n";
    }
    int remain = uc_spy_alive(g) + uc_civ_alive(g);
    e.add_field("存活玩家（" + std::to_string(remain) + " 人）",
        alive_str.empty() ? "（無）" : alive_str, false);

    return dpp::message().add_embed(e);
}

// ─── Game over message ────────────────────────────────────────────────────────

static dpp::message uc_gameover_msg(const UCGame& g, bool civ_win, bool by_guess = false) {
    std::string title;
    if (g.blank_mode) {
        title = civ_win ? "🎉 平民勝利！白板玩家出局！"
                        : (by_guess ? "💥 白板翻盤！猜出正確的詞！"
                                    : "🃏 白板勝利！撐到最後！");
    } else {
        title = civ_win ? "🎉 平民勝利！臥底全部落網！"
                        : (by_guess ? "💥 臥底翻盤！猜出平民詞！"
                                    : "💀 臥底勝利！臥底人數超過平民！");
    }

    dpp::embed e;
    e.set_title(title).set_color(civ_win ? 0x2ECC71 : 0xE74C3C);

    if (g.blank_mode) {
        e.add_field("📖 詞",
            "🌾 平民詞：**" + g.civilian_word + "**\n🃏 白板：（無詞）", false);
    } else {
        e.add_field("📖 詞組",
            "🌾 平民詞：**" + g.civilian_word + "**\n"
            "🕵️ 臥底詞：**" + g.undercover_word + "**", false);
    }

    std::string all_str, winner_str, loser_str;
    for (auto& p : g.players) {
        bool is_spy = p.is_undercover || p.is_blank;
        bool p_wins = (civ_win && !is_spy) || (!civ_win && is_spy);
        std::string icon = p.is_blank ? "🃏" : (p.is_undercover ? "🕵️" : "🌾");
        std::string word = p.is_blank ? "（無詞）"
                         : (p.is_undercover ? g.undercover_word : g.civilian_word);
        std::string dead = p.alive ? "" : "（已出局）";
        all_str += icon + " " + p.display_name + " — " + word + dead + "\n";
        if (p_wins) winner_str += "• " + p.display_name + "\n";
        else        loser_str  += "• " + p.display_name + "\n";
    }
    e.add_field("👥 全員身份揭曉", all_str, false);
    if (!winner_str.empty()) e.add_field("🏆 勝利（+50 碼）", winner_str, true);
    if (!loser_str.empty())  e.add_field("😔 落敗（+20 碼）", loser_str, true);
    e.set_footer(dpp::embed_footer().set_text("遊戲 #" + std::to_string(g.id)));

    dpp::message msg; msg.add_embed(e);
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("🔁 再來一局")
        .set_id((g.adult_allowed ? "uc_adult_again_" : "uc_again_") + std::to_string((uint64_t)g.channel_id) + "_" + std::to_string((uint64_t)g.host_id))
        .set_style(dpp::cos_success));
    msg.add_component(row);
    return msg;
}

// ─── Start a round ────────────────────────────────────────────────────────────

static void uc_start_round(uint64_t gid) {
    UCGame snap;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = uc_games.find(gid);
        if (it == uc_games.end()) return;
        auto& g = it->second;
        g.phase = UCPhase::DESCRIBING;

        if (g.round == 1 || g.seat_order.empty()) {
            // First round: random shuffle becomes permanent seat order
            g.seat_order.clear();
            for (auto& p : g.players) g.seat_order.push_back(p.uid);
            static std::mt19937 rng{std::random_device{}()};
            std::shuffle(g.seat_order.begin(), g.seat_order.end(), rng);
        }

        // Build speak_order from seat_order, skipping dead players
        g.speak_order.clear();
        for (auto uid : g.seat_order) {
            auto* p = uc_find(g, uid);
            if (p && p->alive) g.speak_order.push_back(uid);
        }
        g.speak_pos = 0;
        g.answers.clear();
        g.votes.clear();
        g.pk_candidates.clear();
        g.pk_votes.clear();
        snap = g;
    }
    auto msg = uc_describe_msg(snap);
    msg.channel_id = snap.channel_id;
    g_bot->message_create(msg, [gid](const dpp::confirmation_callback_t& cb) {
        if (cb.is_error()) return;
        std::lock_guard<std::mutex> lk(data_mutex);
        if (uc_games.count(gid))
            uc_games[gid].describe_msg_id = std::get<dpp::message>(cb.value).id;
    });
}

// ─── End game ─────────────────────────────────────────────────────────────────

static void uc_end_game(uint64_t gid, bool civ_win, bool by_guess) {
    UCGame snap;
    std::vector<dpp::snowflake> winners, losers;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = uc_games.find(gid);
        if (it == uc_games.end()) return;
        auto& g = it->second;
        // Cancel pending guess timer if any
        if (g.guess_timer) { g_bot->stop_timer(g.guess_timer); g.guess_timer = 0; }
        snap = g;
        for (auto& p : g.players) {
            bool is_spy = p.is_undercover || p.is_blank;
            bool pw = (civ_win && !is_spy) || (!civ_win && is_spy);
            (pw ? winners : losers).push_back(p.uid);
            auto& s = uc_stats_data[p.uid];
            if (is_spy) { s.spy_games++; if (pw) s.spy_wins++; }
            else        { s.civ_games++; if (pw) s.civ_wins++; }
        }
        channel_uc_game.erase(g.channel_id);
        uc_games.erase(it);
    }
    for (auto u : winners) add_chips(u, 50);
    for (auto u : losers)  add_chips(u, 20);
    save_uc_stats();
    save_chips();

    auto msg = uc_gameover_msg(snap, civ_win, by_guess);
    msg.channel_id = snap.channel_id;
    g_bot->message_create(msg);
}

// ─── Actual elimination (after guess phase or for civilians) ─────────────────

static void uc_do_eliminate_confirmed(uint64_t gid, dpp::snowflake elim_uid) {
    bool over = false, civ_win = false;
    UCGame snap;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = uc_games.find(gid);
        if (it == uc_games.end()) return;
        auto& g = it->second;
        for (auto& p : g.players) if (p.uid == elim_uid) { p.alive = false; break; }
        civ_win = uc_civs_win(g);
        over    = civ_win || uc_spy_wins(g);
        if (over) g.phase = UCPhase::GAME_OVER;
        else      g.round++;
        snap = g;
    }
    auto msg = uc_elim_msg(snap, elim_uid);
    msg.channel_id = snap.channel_id;
    g_bot->message_create(msg);

    if (over) uc_end_game(gid, civ_win, false);
    else      uc_start_round(gid);
}

// ─── Guess phase: show guess button and start 30s timer ──────────────────────

static void uc_start_guess_phase(uint64_t gid, dpp::snowflake elim_uid) {
    dpp::snowflake channel_id;
    std::string elim_name;

    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = uc_games.find(gid);
        if (it == uc_games.end()) return;
        auto& g = it->second;
        for (auto& p : g.players)
            if (p.uid == elim_uid) { elim_name = p.display_name; break; }
        channel_id = g.channel_id;
        g.pending_elim = elim_uid;
    }

    // Start 100-second timer — fires if player doesn't guess in time
    dpp::timer tid = g_bot->start_timer([gid, elim_uid, channel_id](dpp::timer) {
        bool should_elim = false;
        {
            std::lock_guard<std::mutex> lk(data_mutex);
            auto it = uc_games.find(gid);
            if (it == uc_games.end()) return;
            auto& g = it->second;
            if (g.pending_elim == elim_uid) {
                g.pending_elim = 0;
                g.guess_timer  = 0;
                should_elim = true;
            }
        }
        if (should_elim) {
            dpp::message m;
            m.set_content("⏰ 猜詞時間結束，正式淘汰！");
            m.channel_id = channel_id;
            g_bot->message_create(m);
            uc_do_eliminate_confirmed(gid, elim_uid);
        }
    }, 100);

    {
        std::lock_guard<std::mutex> lk(data_mutex);
        if (uc_games.count(gid)) uc_games[gid].guess_timer = tid;
    }

    // Post guess opportunity message in channel
    std::string gs  = std::to_string(gid);
    std::string eu  = std::to_string((uint64_t)elim_uid);
    dpp::embed e;
    e.set_title("💭 猜詞機會！").set_color(0xF39C12);
    e.set_description("**" + elim_name + "** 即將出局！\n\n"
        "<@" + eu + "> 有 **100 秒**猜出平民詞，猜對翻盤！");
    dpp::component row; row.set_type(dpp::cot_action_row);
    row.add_component(dpp::component().set_type(dpp::cot_button)
        .set_label("💭 猜詞")
        .set_id("uc_guess_" + gs + "_" + eu)
        .set_style(dpp::cos_primary));
    dpp::message m; m.add_embed(e); m.add_component(row);
    m.channel_id = channel_id;
    g_bot->message_create(m);
}

// ─── Eliminate one player: civilians go directly, spy/blank gets guess chance ─

static void uc_do_eliminate(uint64_t gid, dpp::snowflake elim_uid) {
    bool is_spy_or_blank = false;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = uc_games.find(gid);
        if (it == uc_games.end()) return;
        for (auto& p : it->second.players)
            if (p.uid == elim_uid) { is_spy_or_blank = p.is_undercover || p.is_blank; break; }
    }
    if (is_spy_or_blank) uc_start_guess_phase(gid, elim_uid);
    else                 uc_do_eliminate_confirmed(gid, elim_uid);
}

// ─── Resolve main vote ────────────────────────────────────────────────────────

static void uc_resolve_vote(uint64_t gid) {
    std::vector<dpp::snowflake> pk_cands;
    dpp::snowflake winner = 0;
    UCGame snap;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = uc_games.find(gid);
        if (it == uc_games.end()) return;
        auto& g = it->second;

        std::map<dpp::snowflake, int> tally;
        for (auto& p : g.players) if (p.alive) tally[p.uid] = 0;
        for (auto& [vt, tg] : g.votes)
            if (tg && tally.count(tg)) tally[tg]++;

        int best = 0;
        for (auto& [u, c] : tally) best = std::max(best, c);
        if (best > 0) {
            for (auto& [u, c] : tally)
                if (c == best) pk_cands.push_back(u);
        }

        if (pk_cands.size() == 1) { winner = pk_cands[0]; pk_cands.clear(); }

        if (!pk_cands.empty()) {
            g.phase = UCPhase::VOTE_PK;
            g.pk_candidates = pk_cands;
            g.pk_votes.clear();
        }
        snap = g;
    }

    if (!pk_cands.empty()) {
        auto pmsg = uc_pk_msg(snap);
        pmsg.channel_id = snap.channel_id;
        g_bot->message_create(pmsg, [gid](const dpp::confirmation_callback_t& cb) {
            if (!cb.is_error()) {
                std::lock_guard<std::mutex> lk(data_mutex);
                if (uc_games.count(gid))
                    uc_games[gid].pk_msg_id = std::get<dpp::message>(cb.value).id;
            }
        });
    } else if (winner) {
        uc_do_eliminate(gid, winner);
    } else {
        dpp::message m; m.set_content("🚫 本輪無人獲得票數，進入下一輪！");
        m.channel_id = snap.channel_id;
        g_bot->message_create(m);
        { std::lock_guard<std::mutex> lk(data_mutex);
          if (uc_games.count(gid)) uc_games[gid].round++; }
        uc_start_round(gid);
    }
}

// ─── Resolve PK vote ──────────────────────────────────────────────────────────

static void uc_resolve_pk(uint64_t gid) {
    dpp::snowflake pk_winner = 0;
    UCGame snap;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = uc_games.find(gid);
        if (it == uc_games.end()) return;
        auto& g = it->second;

        std::map<dpp::snowflake, int> tally;
        for (auto c : g.pk_candidates) tally[c] = 0;
        for (auto& [vt, cd] : g.pk_votes)
            if (tally.count(cd)) tally[cd]++;

        int best = 0;
        for (auto& [u, c] : tally) best = std::max(best, c);
        std::vector<dpp::snowflake> tops;
        for (auto& [u, c] : tally) if (c == best && best > 0) tops.push_back(u);

        if (tops.size() == 1) pk_winner = tops[0];
        snap = g;
    }

    if (pk_winner) {
        uc_do_eliminate(gid, pk_winner);
    } else {
        dpp::message m; m.set_content("⚡ PK 仍然平票！**本輪無人淘汰**，進入下一輪！");
        m.channel_id = snap.channel_id;
        g_bot->message_create(m);
        { std::lock_guard<std::mutex> lk(data_mutex);
          if (uc_games.count(gid)) uc_games[gid].round++; }
        uc_start_round(gid);
    }
}

// ─── Begin game: assign words/roles, send DMs, start first round ─────────────

static void uc_begin_game(uint64_t gid) {
    UCGame snap;
    {
        std::lock_guard<std::mutex> lk(data_mutex);
        auto it = uc_games.find(gid);
        if (it == uc_games.end()) return;
        auto& g = it->second;

        static std::mt19937 rng{std::random_device{}()};
        auto& pool = get_uc_pool(g.word_pool);
        int idx = std::uniform_int_distribution<int>(0, (int)pool.size()-1)(rng);
        if (std::uniform_int_distribution<int>(0,1)(rng)) {
            g.civilian_word   = pool[idx].first;
            g.undercover_word = pool[idx].second;
        } else {
            g.civilian_word   = pool[idx].second;
            g.undercover_word = pool[idx].first;
        }

        if (g.blank_mode) {
            // One blank player (no word), rest civilians
            std::vector<int> roles(g.players.size(), 0);
            roles[0] = 2; // 2 = blank
            std::shuffle(roles.begin(), roles.end(), rng);
            for (size_t i = 0; i < g.players.size(); i++) {
                g.players[i].is_undercover = false;
                g.players[i].is_blank      = (roles[i] == 2);
            }
            g.undercover_word = "";
        } else {
            int n_spy = uc_spy_num((int)g.players.size());
            std::vector<int> roles(g.players.size(), 0);
            for (int i = 0; i < n_spy; i++) roles[i] = 1;
            std::shuffle(roles.begin(), roles.end(), rng);
            for (size_t i = 0; i < g.players.size(); i++) {
                g.players[i].is_undercover = (roles[i] == 1);
                g.players[i].is_blank      = false;
            }
        }

        snap = g;
    }

    // Send DMs — never reveal identity label, just the word
    for (auto& p : snap.players) {
        dpp::embed dm_e;
        if (p.is_blank) {
            dm_e.set_title("🃏 誰是臥底 — 你的牌")
                .set_color(0x95A5A6)
                .set_description("你是白板，需要自己判斷詞是什麼")
                .set_footer(dpp::embed_footer()
                    .set_text("遊戲 #" + std::to_string(snap.id)));
        } else {
            std::string word = p.is_undercover ? snap.undercover_word : snap.civilian_word;
            dm_e.set_title("🕵️ 誰是臥底 — 你的詞")
                .set_color(0x3498DB)
                .set_description("你的詞是：\n# " + word)
                .set_footer(dpp::embed_footer()
                    .set_text("⚠️ 不能直接說出詞本身！遊戲 #" + std::to_string(snap.id)));
        }
        g_bot->direct_message_create(p.uid, dpp::message().add_embed(dm_e));
    }

    dpp::message note;
    note.set_content("📨 詞已透過私訊發送給所有玩家！若未收到，請確認已開放伺服器成員私訊。");
    note.channel_id = snap.channel_id;
    g_bot->message_create(note);

    uc_start_round(gid);
}
