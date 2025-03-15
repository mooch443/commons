#include "colors.h"


namespace cmn::gui {

Color Color::fromStr(const std::string& str) {
    auto s = utils::lowercase(str);
    if (s == "red") return gui::Red;
    if (s == "blue") return gui::Blue;
    if (s == "green") return gui::Green;
    if (s == "darkgreen") return gui::DarkGreen;
    if (s == "yellow") return gui::Yellow;
    if (s == "cyan") return gui::Cyan;
    if (s == "white") return gui::White;
    if (s == "black") return gui::Black;
    if (s == "gray") return gui::Gray;
    if (s == "darkgray") return gui::DarkGray;
    if (s == "lightgray") return gui::LightGray;
    if (s == "transparent") return gui::Transparent;
    if (s == "purple") return gui::Purple;

    auto vec = Meta::fromStr<std::vector<uchar>>(str);
    if (vec.empty())
        return Color();
    if (vec.size() != 4 && vec.size() != 3)
        throw CustomException(type_v<std::invalid_argument>, "Can only initialize Color with three or four elements. (", str, ")");
    return Color(vec[0], vec[1], vec[2], vec.size() == 4 ? vec[3] : 255);
}

}

namespace cmn::cmap {
IMPLEMENT(Viridis::data_bgr){{
 Viridis::value_t {0.26700401,  0.00487433,  0.32941519},
 Viridis::value_t {0.26851048,  0.00960483,  0.33542652},
 Viridis::value_t {0.26994384,  0.01462494,  0.34137895},
 Viridis::value_t {0.27130489,  0.01994186,  0.34726862},
 Viridis::value_t {0.27259384,  0.02556309,  0.35309303},
 Viridis::value_t {0.27380934,  0.03149748,  0.35885256},
 Viridis::value_t {0.27495242,  0.03775181,  0.36454323},
 Viridis::value_t {0.27602238,  0.04416723,  0.37016418},
 Viridis::value_t {0.2770184 ,  0.05034437,  0.37571452},
 Viridis::value_t {0.27794143,  0.05632444,  0.38119074},
 Viridis::value_t {0.27879067,  0.06214536,  0.38659204},
 Viridis::value_t {0.2795655 ,  0.06783587,  0.39191723},
 Viridis::value_t {0.28026658,  0.07341724,  0.39716349},
 Viridis::value_t {0.28089358,  0.07890703,  0.40232944},
 Viridis::value_t {0.28144581,  0.0843197 ,  0.40741404},
 Viridis::value_t {0.28192358,  0.08966622,  0.41241521},
 Viridis::value_t {0.28232739,  0.09495545,  0.41733086},
 Viridis::value_t {0.28265633,  0.10019576,  0.42216032},
 Viridis::value_t {0.28291049,  0.10539345,  0.42690202},
 Viridis::value_t {0.28309095,  0.11055307,  0.43155375},
 Viridis::value_t {0.28319704,  0.11567966,  0.43611482},
 Viridis::value_t {0.28322882,  0.12077701,  0.44058404},
 Viridis::value_t {0.28318684,  0.12584799,  0.44496   },
 Viridis::value_t {0.283072  ,  0.13089477,  0.44924127},
 Viridis::value_t {0.28288389,  0.13592005,  0.45342734},
 Viridis::value_t {0.28262297,  0.14092556,  0.45751726},
 Viridis::value_t {0.28229037,  0.14591233,  0.46150995},
 Viridis::value_t {0.28188676,  0.15088147,  0.46540474},
 Viridis::value_t {0.28141228,  0.15583425,  0.46920128},
 Viridis::value_t {0.28086773,  0.16077132,  0.47289909},
 Viridis::value_t {0.28025468,  0.16569272,  0.47649762},
 Viridis::value_t {0.27957399,  0.17059884,  0.47999675},
 Viridis::value_t {0.27882618,  0.1754902 ,  0.48339654},
 Viridis::value_t {0.27801236,  0.18036684,  0.48669702},
 Viridis::value_t {0.27713437,  0.18522836,  0.48989831},
 Viridis::value_t {0.27619376,  0.19007447,  0.49300074},
 Viridis::value_t {0.27519116,  0.1949054 ,  0.49600488},
 Viridis::value_t {0.27412802,  0.19972086,  0.49891131},
 Viridis::value_t {0.27300596,  0.20452049,  0.50172076},
 Viridis::value_t {0.27182812,  0.20930306,  0.50443413},
 Viridis::value_t {0.27059473,  0.21406899,  0.50705243},
 Viridis::value_t {0.26930756,  0.21881782,  0.50957678},
 Viridis::value_t {0.26796846,  0.22354911,  0.5120084 },
 Viridis::value_t {0.26657984,  0.2282621 ,  0.5143487 },
 Viridis::value_t {0.2651445 ,  0.23295593,  0.5165993 },
 Viridis::value_t {0.2636632 ,  0.23763078,  0.51876163},
 Viridis::value_t {0.26213801,  0.24228619,  0.52083736},
 Viridis::value_t {0.26057103,  0.2469217 ,  0.52282822},
 Viridis::value_t {0.25896451,  0.25153685,  0.52473609},
 Viridis::value_t {0.25732244,  0.2561304 ,  0.52656332},
 Viridis::value_t {0.25564519,  0.26070284,  0.52831152},
 Viridis::value_t {0.25393498,  0.26525384,  0.52998273},
 Viridis::value_t {0.25219404,  0.26978306,  0.53157905},
 Viridis::value_t {0.25042462,  0.27429024,  0.53310261},
 Viridis::value_t {0.24862899,  0.27877509,  0.53455561},
 Viridis::value_t {0.2468114 ,  0.28323662,  0.53594093},
 Viridis::value_t {0.24497208,  0.28767547,  0.53726018},
 Viridis::value_t {0.24311324,  0.29209154,  0.53851561},
 Viridis::value_t {0.24123708,  0.29648471,  0.53970946},
 Viridis::value_t {0.23934575,  0.30085494,  0.54084398},
 Viridis::value_t {0.23744138,  0.30520222,  0.5419214 },
 Viridis::value_t {0.23552606,  0.30952657,  0.54294396},
 Viridis::value_t {0.23360277,  0.31382773,  0.54391424},
 Viridis::value_t {0.2316735 ,  0.3181058 ,  0.54483444},
 Viridis::value_t {0.22973926,  0.32236127,  0.54570633},
 Viridis::value_t {0.22780192,  0.32659432,  0.546532  },
 Viridis::value_t {0.2258633 ,  0.33080515,  0.54731353},
 Viridis::value_t {0.22392515,  0.334994  ,  0.54805291},
 Viridis::value_t {0.22198915,  0.33916114,  0.54875211},
 Viridis::value_t {0.22005691,  0.34330688,  0.54941304},
 Viridis::value_t {0.21812995,  0.34743154,  0.55003755},
 Viridis::value_t {0.21620971,  0.35153548,  0.55062743},
 Viridis::value_t {0.21429757,  0.35561907,  0.5511844 },
 Viridis::value_t {0.21239477,  0.35968273,  0.55171011},
 Viridis::value_t {0.2105031 ,  0.36372671,  0.55220646},
 Viridis::value_t {0.20862342,  0.36775151,  0.55267486},
 Viridis::value_t {0.20675628,  0.37175775,  0.55311653},
 Viridis::value_t {0.20490257,  0.37574589,  0.55353282},
 Viridis::value_t {0.20306309,  0.37971644,  0.55392505},
 Viridis::value_t {0.20123854,  0.38366989,  0.55429441},
 Viridis::value_t {0.1994295 ,  0.38760678,  0.55464205},
 Viridis::value_t {0.1976365 ,  0.39152762,  0.55496905},
 Viridis::value_t {0.19585993,  0.39543297,  0.55527637},
 Viridis::value_t {0.19410009,  0.39932336,  0.55556494},
 Viridis::value_t {0.19235719,  0.40319934,  0.55583559},
 Viridis::value_t {0.19063135,  0.40706148,  0.55608907},
 Viridis::value_t {0.18892259,  0.41091033,  0.55632606},
 Viridis::value_t {0.18723083,  0.41474645,  0.55654717},
 Viridis::value_t {0.18555593,  0.4185704 ,  0.55675292},
 Viridis::value_t {0.18389763,  0.42238275,  0.55694377},
 Viridis::value_t {0.18225561,  0.42618405,  0.5571201 },
 Viridis::value_t {0.18062949,  0.42997486,  0.55728221},
 Viridis::value_t {0.17901879,  0.43375572,  0.55743035},
 Viridis::value_t {0.17742298,  0.4375272 ,  0.55756466},
 Viridis::value_t {0.17584148,  0.44128981,  0.55768526},
 Viridis::value_t {0.17427363,  0.4450441 ,  0.55779216},
 Viridis::value_t {0.17271876,  0.4487906 ,  0.55788532},
 Viridis::value_t {0.17117615,  0.4525298 ,  0.55796464},
 Viridis::value_t {0.16964573,  0.45626209,  0.55803034},
 Viridis::value_t {0.16812641,  0.45998802,  0.55808199},
 Viridis::value_t {0.1666171 ,  0.46370813,  0.55811913},
 Viridis::value_t {0.16511703,  0.4674229 ,  0.55814141},
 Viridis::value_t {0.16362543,  0.47113278,  0.55814842},
 Viridis::value_t {0.16214155,  0.47483821,  0.55813967},
 Viridis::value_t {0.16066467,  0.47853961,  0.55811466},
 Viridis::value_t {0.15919413,  0.4822374 ,  0.5580728 },
 Viridis::value_t {0.15772933,  0.48593197,  0.55801347},
 Viridis::value_t {0.15626973,  0.4896237 ,  0.557936  },
 Viridis::value_t {0.15481488,  0.49331293,  0.55783967},
 Viridis::value_t {0.15336445,  0.49700003,  0.55772371},
 Viridis::value_t {0.1519182 ,  0.50068529,  0.55758733},
 Viridis::value_t {0.15047605,  0.50436904,  0.55742968},
 Viridis::value_t {0.14903918,  0.50805136,  0.5572505 },
 Viridis::value_t {0.14760731,  0.51173263,  0.55704861},
 Viridis::value_t {0.14618026,  0.51541316,  0.55682271},
 Viridis::value_t {0.14475863,  0.51909319,  0.55657181},
 Viridis::value_t {0.14334327,  0.52277292,  0.55629491},
 Viridis::value_t {0.14193527,  0.52645254,  0.55599097},
 Viridis::value_t {0.14053599,  0.53013219,  0.55565893},
 Viridis::value_t {0.13914708,  0.53381201,  0.55529773},
 Viridis::value_t {0.13777048,  0.53749213,  0.55490625},
 Viridis::value_t {0.1364085 ,  0.54117264,  0.55448339},
 Viridis::value_t {0.13506561,  0.54485335,  0.55402906},
 Viridis::value_t {0.13374299,  0.54853458,  0.55354108},
 Viridis::value_t {0.13244401,  0.55221637,  0.55301828},
 Viridis::value_t {0.13117249,  0.55589872,  0.55245948},
 Viridis::value_t {0.1299327 ,  0.55958162,  0.55186354},
 Viridis::value_t {0.12872938,  0.56326503,  0.55122927},
 Viridis::value_t {0.12756771,  0.56694891,  0.55055551},
 Viridis::value_t {0.12645338,  0.57063316,  0.5498411 },
 Viridis::value_t {0.12539383,  0.57431754,  0.54908564},
 Viridis::value_t {0.12439474,  0.57800205,  0.5482874 },
 Viridis::value_t {0.12346281,  0.58168661,  0.54744498},
 Viridis::value_t {0.12260562,  0.58537105,  0.54655722},
 Viridis::value_t {0.12183122,  0.58905521,  0.54562298},
 Viridis::value_t {0.12114807,  0.59273889,  0.54464114},
 Viridis::value_t {0.12056501,  0.59642187,  0.54361058},
 Viridis::value_t {0.12009154,  0.60010387,  0.54253043},
 Viridis::value_t {0.11973756,  0.60378459,  0.54139999},
 Viridis::value_t {0.11951163,  0.60746388,  0.54021751},
 Viridis::value_t {0.11942341,  0.61114146,  0.53898192},
 Viridis::value_t {0.11948255,  0.61481702,  0.53769219},
 Viridis::value_t {0.11969858,  0.61849025,  0.53634733},
 Viridis::value_t {0.12008079,  0.62216081,  0.53494633},
 Viridis::value_t {0.12063824,  0.62582833,  0.53348834},
 Viridis::value_t {0.12137972,  0.62949242,  0.53197275},
 Viridis::value_t {0.12231244,  0.63315277,  0.53039808},
 Viridis::value_t {0.12344358,  0.63680899,  0.52876343},
 Viridis::value_t {0.12477953,  0.64046069,  0.52706792},
 Viridis::value_t {0.12632581,  0.64410744,  0.52531069},
 Viridis::value_t {0.12808703,  0.64774881,  0.52349092},
 Viridis::value_t {0.13006688,  0.65138436,  0.52160791},
 Viridis::value_t {0.13226797,  0.65501363,  0.51966086},
 Viridis::value_t {0.13469183,  0.65863619,  0.5176488 },
 Viridis::value_t {0.13733921,  0.66225157,  0.51557101},
 Viridis::value_t {0.14020991,  0.66585927,  0.5134268 },
 Viridis::value_t {0.14330291,  0.66945881,  0.51121549},
 Viridis::value_t {0.1466164 ,  0.67304968,  0.50893644},
 Viridis::value_t {0.15014782,  0.67663139,  0.5065889 },
 Viridis::value_t {0.15389405,  0.68020343,  0.50417217},
 Viridis::value_t {0.15785146,  0.68376525,  0.50168574},
 Viridis::value_t {0.16201598,  0.68731632,  0.49912906},
 Viridis::value_t {0.1663832 ,  0.69085611,  0.49650163},
 Viridis::value_t {0.1709484 ,  0.69438405,  0.49380294},
 Viridis::value_t {0.17570671,  0.6978996 ,  0.49103252},
 Viridis::value_t {0.18065314,  0.70140222,  0.48818938},
 Viridis::value_t {0.18578266,  0.70489133,  0.48527326},
 Viridis::value_t {0.19109018,  0.70836635,  0.48228395},
 Viridis::value_t {0.19657063,  0.71182668,  0.47922108},
 Viridis::value_t {0.20221902,  0.71527175,  0.47608431},
 Viridis::value_t {0.20803045,  0.71870095,  0.4728733 },
 Viridis::value_t {0.21400015,  0.72211371,  0.46958774},
 Viridis::value_t {0.22012381,  0.72550945,  0.46622638},
 Viridis::value_t {0.2263969 ,  0.72888753,  0.46278934},
 Viridis::value_t {0.23281498,  0.73224735,  0.45927675},
 Viridis::value_t {0.2393739 ,  0.73558828,  0.45568838},
 Viridis::value_t {0.24606968,  0.73890972,  0.45202405},
 Viridis::value_t {0.25289851,  0.74221104,  0.44828355},
 Viridis::value_t {0.25985676,  0.74549162,  0.44446673},
 Viridis::value_t {0.26694127,  0.74875084,  0.44057284},
 Viridis::value_t {0.27414922,  0.75198807,  0.4366009 },
 Viridis::value_t {0.28147681,  0.75520266,  0.43255207},
 Viridis::value_t {0.28892102,  0.75839399,  0.42842626},
 Viridis::value_t {0.29647899,  0.76156142,  0.42422341},
 Viridis::value_t {0.30414796,  0.76470433,  0.41994346},
 Viridis::value_t {0.31192534,  0.76782207,  0.41558638},
 Viridis::value_t {0.3198086 ,  0.77091403,  0.41115215},
 Viridis::value_t {0.3277958 ,  0.77397953,  0.40664011},
 Viridis::value_t {0.33588539,  0.7770179 ,  0.40204917},
 Viridis::value_t {0.34407411,  0.78002855,  0.39738103},
 Viridis::value_t {0.35235985,  0.78301086,  0.39263579},
 Viridis::value_t {0.36074053,  0.78596419,  0.38781353},
 Viridis::value_t {0.3692142 ,  0.78888793,  0.38291438},
 Viridis::value_t {0.37777892,  0.79178146,  0.3779385 },
 Viridis::value_t {0.38643282,  0.79464415,  0.37288606},
 Viridis::value_t {0.39517408,  0.79747541,  0.36775726},
 Viridis::value_t {0.40400101,  0.80027461,  0.36255223},
 Viridis::value_t {0.4129135 ,  0.80304099,  0.35726893},
 Viridis::value_t {0.42190813,  0.80577412,  0.35191009},
 Viridis::value_t {0.43098317,  0.80847343,  0.34647607},
 Viridis::value_t {0.44013691,  0.81113836,  0.3409673 },
 Viridis::value_t {0.44936763,  0.81376835,  0.33538426},
 Viridis::value_t {0.45867362,  0.81636288,  0.32972749},
 Viridis::value_t {0.46805314,  0.81892143,  0.32399761},
 Viridis::value_t {0.47750446,  0.82144351,  0.31819529},
 Viridis::value_t {0.4870258 ,  0.82392862,  0.31232133},
 Viridis::value_t {0.49661536,  0.82637633,  0.30637661},
 Viridis::value_t {0.5062713 ,  0.82878621,  0.30036211},
 Viridis::value_t {0.51599182,  0.83115784,  0.29427888},
 Viridis::value_t {0.52577622,  0.83349064,  0.2881265 },
 Viridis::value_t {0.5356211 ,  0.83578452,  0.28190832},
 Viridis::value_t {0.5455244 ,  0.83803918,  0.27562602},
 Viridis::value_t {0.55548397,  0.84025437,  0.26928147},
 Viridis::value_t {0.5654976 ,  0.8424299 ,  0.26287683},
 Viridis::value_t {0.57556297,  0.84456561,  0.25641457},
 Viridis::value_t {0.58567772,  0.84666139,  0.24989748},
 Viridis::value_t {0.59583934,  0.84871722,  0.24332878},
 Viridis::value_t {0.60604528,  0.8507331 ,  0.23671214},
 Viridis::value_t {0.61629283,  0.85270912,  0.23005179},
 Viridis::value_t {0.62657923,  0.85464543,  0.22335258},
 Viridis::value_t {0.63690157,  0.85654226,  0.21662012},
 Viridis::value_t {0.64725685,  0.85839991,  0.20986086},
 Viridis::value_t {0.65764197,  0.86021878,  0.20308229},
 Viridis::value_t {0.66805369,  0.86199932,  0.19629307},
 Viridis::value_t {0.67848868,  0.86374211,  0.18950326},
 Viridis::value_t {0.68894351,  0.86544779,  0.18272455},
 Viridis::value_t {0.69941463,  0.86711711,  0.17597055},
 Viridis::value_t {0.70989842,  0.86875092,  0.16925712},
 Viridis::value_t {0.72039115,  0.87035015,  0.16260273},
 Viridis::value_t {0.73088902,  0.87191584,  0.15602894},
 Viridis::value_t {0.74138803,  0.87344918,  0.14956101},
 Viridis::value_t {0.75188414,  0.87495143,  0.14322828},
 Viridis::value_t {0.76237342,  0.87642392,  0.13706449},
 Viridis::value_t {0.77285183,  0.87786808,  0.13110864},
 Viridis::value_t {0.78331535,  0.87928545,  0.12540538},
 Viridis::value_t {0.79375994,  0.88067763,  0.12000532},
 Viridis::value_t {0.80418159,  0.88204632,  0.11496505},
 Viridis::value_t {0.81457634,  0.88339329,  0.11034678},
 Viridis::value_t {0.82494028,  0.88472036,  0.10621724},
 Viridis::value_t {0.83526959,  0.88602943,  0.1026459 },
 Viridis::value_t {0.84556056,  0.88732243,  0.09970219},
 Viridis::value_t {0.8558096 ,  0.88860134,  0.09745186},
 Viridis::value_t {0.86601325,  0.88986815,  0.09595277},
 Viridis::value_t {0.87616824,  0.89112487,  0.09525046},
 Viridis::value_t {0.88627146,  0.89237353,  0.09537439},
 Viridis::value_t {0.89632002,  0.89361614,  0.09633538},
 Viridis::value_t {0.90631121,  0.89485467,  0.09812496},
 Viridis::value_t {0.91624212,  0.89609127,  0.1007168 },
 Viridis::value_t {0.92610579,  0.89732977,  0.10407067},
 Viridis::value_t {0.93590444,  0.8985704 ,  0.10813094},
 Viridis::value_t {0.94563626,  0.899815  ,  0.11283773},
 Viridis::value_t {0.95529972,  0.90106534,  0.11812832},
 Viridis::value_t {0.96489353,  0.90232311,  0.12394051},
 Viridis::value_t {0.97441665,  0.90358991,  0.13021494},
 Viridis::value_t {0.98386829,  0.90486726,  0.13689671},
 Viridis::value_t {0.99324789,  0.90615657,  0.1439362 }
}};

gui::Color Viridis::value(double percent) {
 size_t index = size_t(min(1.0, cmn::abs(percent)) * 255);
 auto& [r, g, b] = data_bgr[index];
 
 return gui::Color((uint8_t)saturate(r * 255), (uint8_t)saturate(g * 255), (uint8_t)saturate(b * 255), 255);
}

IMPLEMENT(BlackToPink::data_bgr){{
 {40.0, 43.0, 46.0},   // #2e2b28  -> (R=46, G=43, B=40)
 {52.0, 55.0, 59.0},   // #3b3734  -> (R=59, G=55, B=52)
 {64.0, 68.0, 71.0},   // #474440  -> (R=71, G=68, B=64)
 {76.0, 80.0, 84.0},   // #54504c  -> (R=84, G=80, B=76)
 {107.0, 80.0, 107.0}, // #6b506b  -> (R=107, G=80, B=107)
 {169.0, 61.0, 171.0}, // #ab3da9  -> (R=171, G=61, B=169)
 {218.0, 37.0, 222.0}, // #de25da  -> (R=222, G=37, B=218)
 {232.0, 68.0, 235.0}, // #eb44e8  -> (R=235, G=68, B=232)
 {255.0, 128.0, 255.0} // #ff80ff  -> (R=255, G=128, B=255)
}};

// Returns a gui::Color interpolated along the palette based on percent (0.0 to 1.0).
gui::Color BlackToPink::value(double percent) {
 // Clamp the percentage to [0, 1]
 if (percent < 0.0) { percent = 0.0; }
 if (percent > 1.0) { percent = 1.0; }
 
 // Scale to the palette range
 double pos = percent * (data_bgr.size() - 1);
 size_t idx = static_cast<size_t>(pos);
 double t = pos - idx;
 
 double b, g, r;
 if (idx >= data_bgr.size() - 1) {
     // If at the end, just use the last color.
     b = std::get<0>(data_bgr.back());
     g = std::get<1>(data_bgr.back());
     r = std::get<2>(data_bgr.back());
 } else {
     // Interpolate linearly between data_bgr[idx] and data_bgr[idx+1].
     const auto& c0 = data_bgr[idx];
     const auto& c1 = data_bgr[idx + 1];
     b = (1.0 - t) * std::get<0>(c0) + t * std::get<0>(c1);
     g = (1.0 - t) * std::get<1>(c0) + t * std::get<1>(c1);
     r = (1.0 - t) * std::get<2>(c0) + t * std::get<2>(c1);
 }
 
 // Convert to integer values with rounding.
 int R = static_cast<int>(std::round(r));
 int G = static_cast<int>(std::round(g));
 int B = static_cast<int>(std::round(b));
 
 // Construct and return a gui::Color.
 // (Assuming gui::Color's constructor takes (R, G, B))
 return gui::Color(R, G, B);
}

// Implementation for BlueToRed palette
IMPLEMENT(BlueToRed::data_bgr){{
 {197.0, 132.0, 25.0},   // #1984c5 -> (R=25, G=132, B=197)
 {240.0, 167.0, 34.0},   // #22a7f0 -> (R=34, G=167, B=240)
 {240.0, 191.0, 99.0},   // #63bff0 -> (R=99, G=191, B=240)
 {237.0, 213.0, 167.0},  // #a7d5ed -> (R=167, G=213, B=237)
 {226.0, 226.0, 226.0},  // #e2e2e2 -> (R=226, G=226, B=226)
 {146.0, 166.0, 225.0},  // #e1a692 -> (R=225, G=166, B=146)
 {86.0, 110.0, 222.0},   // #de6e56 -> (R=222, G=110, B=86)
 {49.0, 75.0, 225.0},    // #e14b31 -> (R=225, G=75, B=49)
 {40.0, 55.0, 194.0}     // #c23728 -> (R=194, G=55, B=40)
}};

gui::Color BlueToRed::value(double percent) {
 if (percent < 0.0) { percent = 0.0; }
 if (percent > 1.0) { percent = 1.0; }
 
 double pos = percent * (data_bgr.size() - 1);
 size_t idx = static_cast<size_t>(pos);
 double t = pos - idx;
 
 double b, g, r;
 if (idx >= data_bgr.size() - 1) {
     b = std::get<0>(data_bgr.back());
     g = std::get<1>(data_bgr.back());
     r = std::get<2>(data_bgr.back());
 } else {
     const auto& c0 = data_bgr[idx];
     const auto& c1 = data_bgr[idx + 1];
     b = (1.0 - t) * std::get<0>(c0) + t * std::get<0>(c1);
     g = (1.0 - t) * std::get<1>(c0) + t * std::get<1>(c1);
     r = (1.0 - t) * std::get<2>(c0) + t * std::get<2>(c1);
 }
 
 int R = static_cast<int>(std::round(r));
 int G = static_cast<int>(std::round(g));
 int B = static_cast<int>(std::round(b));
 return gui::Color(R, G, B);
}

// Implementation for PinkFoam palette
IMPLEMENT(PinkFoam::data_bgr){{
 {190.0, 190.0, 84.0},   // #54bebe -> (R=84, G=190, B=190)
 {200.0, 200.0, 118.0},  // #76c8c8 -> (R=118, G=200, B=200)
 {209.0, 209.0, 152.0},  // #98d1d1 -> (R=152, G=209, B=209)
 {219.0, 219.0, 186.0},  // #badbdb -> (R=186, G=219, B=219)
 {210.0, 218.0, 222.0},  // #dedad2 -> (R=222, G=218, B=210)
 {173.0, 188.0, 228.0},  // #e4bcad -> (R=228, G=188, B=173)
 {158.0, 151.0, 223.0},  // #df979e -> (R=223, G=151, B=158)
 {139.0, 101.0, 215.0},  // #d7658b -> (R=215, G=101, B=139)
 {100.0, 0.0, 200.0}     // #c80064 -> (R=200, G=0, B=100)
}};

gui::Color PinkFoam::value(double percent) {
 if (percent < 0.0) { percent = 0.0; }
 if (percent > 1.0) { percent = 1.0; }
 
 double pos = percent * (data_bgr.size() - 1);
 size_t idx = static_cast<size_t>(pos);
 double t = pos - idx;
 
 double b, g, r;
 if (idx >= data_bgr.size() - 1) {
     b = std::get<0>(data_bgr.back());
     g = std::get<1>(data_bgr.back());
     r = std::get<2>(data_bgr.back());
 } else {
     const auto& c0 = data_bgr[idx];
     const auto& c1 = data_bgr[idx + 1];
     b = (1.0 - t) * std::get<0>(c0) + t * std::get<0>(c1);
     g = (1.0 - t) * std::get<1>(c0) + t * std::get<1>(c1);
     r = (1.0 - t) * std::get<2>(c0) + t * std::get<2>(c1);
 }
 
 int R = static_cast<int>(std::round(r));
 int G = static_cast<int>(std::round(g));
 int B = static_cast<int>(std::round(b));
 return gui::Color(R, G, B);
}

// Implementation for BlueToYellow palette
IMPLEMENT(BlueToYellow::data_bgr){{
 {154.0, 95.0, 17.0},   // #115f9a -> (R=17, G=95, B=154)
 {197.0, 132.0, 25.0},   // #1984c5 -> (R=25, G=132, B=197)
 {240.0, 167.0, 34.0},   // #22a7f0 -> (R=34, G=167, B=240)
 {196.0, 181.0, 72.0},   // #48b5c4 -> (R=72, G=181, B=196)
 {143.0, 198.0, 118.0},  // #76c68f -> (R=118, G=198, B=143)
 {91.0, 215.0, 166.0},   // #a6d75b -> (R=166, G=215, B=91)
 {47.0, 229.0, 201.0},   // #c9e52f -> (R=201, G=229, B=47)
 {17.0, 238.0, 208.0},   // #d0ee11 -> (R=208, G=238, B=17)
 {0.0, 244.0, 208.0}     // #d0f400 -> (R=208, G=244, B=0)
}};

gui::Color BlueToYellow::value(double percent) {
 if (percent < 0.0) { percent = 0.0; }
 if (percent > 1.0) { percent = 1.0; }
 
 double pos = percent * (data_bgr.size() - 1);
 size_t idx = static_cast<size_t>(pos);
 double t = pos - idx;
 
 double b, g, r;
 if (idx >= data_bgr.size() - 1) {
     b = std::get<0>(data_bgr.back());
     g = std::get<1>(data_bgr.back());
     r = std::get<2>(data_bgr.back());
 } else {
     const auto& c0 = data_bgr[idx];
     const auto& c1 = data_bgr[idx + 1];
     b = (1.0 - t) * std::get<0>(c0) + t * std::get<0>(c1);
     g = (1.0 - t) * std::get<1>(c0) + t * std::get<1>(c1);
     r = (1.0 - t) * std::get<2>(c0) + t * std::get<2>(c1);
 }
 int R = static_cast<int>(std::round(r));
 int G = static_cast<int>(std::round(g));
 int B = static_cast<int>(std::round(b));
 return gui::Color(R, G, B);
}

IMPLEMENT(BlackToWhite::data_bgr){{
 {0.0,   0.0,   0.0},     // #000000
 {32.0,  32.0,  32.0},    // #202020
 {64.0,  64.0,  64.0},    // #404040
 {96.0,  96.0,  96.0},    // #606060
 {128.0, 128.0, 128.0},   // #808080
 {160.0, 160.0, 160.0},   // #A0A0A0
 {192.0, 192.0, 192.0},   // #C0C0C0
 {224.0, 224.0, 224.0},   // #E0E0E0
 {255.0, 255.0, 255.0}    // #FFFFFF
}};

gui::Color BlackToWhite::value(double percent) {
 if (percent < 0.0) { percent = 0.0; }
 if (percent > 1.0) { percent = 1.0; }
 
 double pos = percent * (data_bgr.size() - 1);
 size_t idx = static_cast<size_t>(pos);
 double t = pos - idx;
 
 double b, g, r;
 if (idx >= data_bgr.size() - 1) {
     b = std::get<0>(data_bgr.back());
     g = std::get<1>(data_bgr.back());
     r = std::get<2>(data_bgr.back());
 } else {
     const auto& c0 = data_bgr[idx];
     const auto& c1 = data_bgr[idx + 1];
     b = (1.0 - t) * std::get<0>(c0) + t * std::get<0>(c1);
     g = (1.0 - t) * std::get<1>(c0) + t * std::get<1>(c1);
     r = (1.0 - t) * std::get<2>(c0) + t * std::get<2>(c1);
 }
 
 int R = static_cast<int>(std::round(r));
 int G = static_cast<int>(std::round(g));
 int B = static_cast<int>(std::round(b));
 return gui::Color(R, G, B);
}

// Implementation for BlackToCyan palette
IMPLEMENT(BlackToCyan::data_bgr){{
    {0.0,   0.0,   0.0},   // Black: (B=0, G=0, R=0)
    {32.0,  32.0,  0.0},
    {64.0,  64.0,  0.0},
    {96.0,  96.0,  0.0},
    {128.0, 128.0, 0.0},
    {160.0, 160.0, 0.0},
    {192.0, 192.0, 0.0},
    {224.0, 224.0, 0.0},
    {255.0, 255.0, 0.0}    // Cyan in BGR: (B=255, G=255, R=0)
}};

gui::Color BlackToCyan::value(double percent) {
    if (percent < 0.0) { percent = 0.0; }
    if (percent > 1.0) { percent = 1.0; }
    double pos = percent * (data_bgr.size() - 1);
    size_t idx = static_cast<size_t>(pos);
    double t = pos - idx;
    double b, g, r;
    if (idx >= data_bgr.size() - 1) {
        b = std::get<0>(data_bgr.back());
        g = std::get<1>(data_bgr.back());
        r = std::get<2>(data_bgr.back());
    } else {
        const auto& c0 = data_bgr[idx];
        const auto& c1 = data_bgr[idx + 1];
        b = (1.0 - t) * std::get<0>(c0) + t * std::get<0>(c1);
        g = (1.0 - t) * std::get<1>(c0) + t * std::get<1>(c1);
        r = (1.0 - t) * std::get<2>(c0) + t * std::get<2>(c1);
    }
    int R = static_cast<int>(std::round(r));
    int G = static_cast<int>(std::round(g));
    int B = static_cast<int>(std::round(b));
    return gui::Color(R, G, B);
}

// Implementation for BlueToCyan palette
IMPLEMENT(BlueToCyan::data_bgr){{
    {255.0, 0.0,   0.0},   // Blue in BGR: (B=255, G=0, R=0)
    {255.0, 32.0,  0.0},
    {255.0, 64.0,  0.0},
    {255.0, 96.0,  0.0},
    {255.0, 128.0, 0.0},
    {255.0, 160.0, 0.0},
    {255.0, 192.0, 0.0},
    {255.0, 224.0, 0.0},
    {255.0, 255.0, 0.0}    // Cyan in BGR: (B=255, G=255, R=0)
}};

gui::Color BlueToCyan::value(double percent) {
    if (percent < 0.0) { percent = 0.0; }
    if (percent > 1.0) { percent = 1.0; }
    double pos = percent * (data_bgr.size() - 1);
    size_t idx = static_cast<size_t>(pos);
    double t = pos - idx;
    double b, g, r;
    if (idx >= data_bgr.size() - 1) {
        b = std::get<0>(data_bgr.back());
        g = std::get<1>(data_bgr.back());
        r = std::get<2>(data_bgr.back());
    } else {
        const auto& c0 = data_bgr[idx];
        const auto& c1 = data_bgr[idx + 1];
        b = (1.0 - t) * std::get<0>(c0) + t * std::get<0>(c1);
        g = (1.0 - t) * std::get<1>(c0) + t * std::get<1>(c1);
        r = (1.0 - t) * std::get<2>(c0) + t * std::get<2>(c1);
    }
    int R = static_cast<int>(std::round(r));
    int G = static_cast<int>(std::round(g));
    int B = static_cast<int>(std::round(b));
    return gui::Color(R, G, B);
}

// Implementation for BlackToGreen palette
IMPLEMENT(BlackToGreen::data_bgr){{
    {0.0,   0.0, 0.0},     // Black: (B=0, G=0, R=0)
    {0.0,  32.0, 0.0},
    {0.0,  64.0, 0.0},
    {0.0,  96.0, 0.0},
    {0.0, 128.0, 0.0},
    {0.0, 160.0, 0.0},
    {0.0, 192.0, 0.0},
    {0.0, 224.0, 0.0},
    {0.0, 255.0, 0.0}      // Green in BGR: (B=0, G=255, R=0)
}};

gui::Color BlackToGreen::value(double percent) {
    if (percent < 0.0) { percent = 0.0; }
    if (percent > 1.0) { percent = 1.0; }
    double pos = percent * (data_bgr.size() - 1);
    size_t idx = static_cast<size_t>(pos);
    double t = pos - idx;
    double b, g, r;
    if (idx >= data_bgr.size() - 1) {
        b = std::get<0>(data_bgr.back());
        g = std::get<1>(data_bgr.back());
        r = std::get<2>(data_bgr.back());
    } else {
        const auto& c0 = data_bgr[idx];
        const auto& c1 = data_bgr[idx + 1];
        b = (1.0 - t) * std::get<0>(c0) + t * std::get<0>(c1);
        g = (1.0 - t) * std::get<1>(c0) + t * std::get<1>(c1);
        r = (1.0 - t) * std::get<2>(c0) + t * std::get<2>(c1);
    }
    int R = static_cast<int>(std::round(r));
    int G = static_cast<int>(std::round(g));
    int B = static_cast<int>(std::round(b));
    return gui::Color(R, G, B);
}

} /// EOF cmap
