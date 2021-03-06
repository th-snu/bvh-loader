#include "BVHReader.h"
#include <stack>

using namespace std;

static inline void trim(std::string& s) {
    replace(s.begin(), s.end(), '\r', ' ');
    replace(s.begin(), s.end(), '\t', ' ');
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
		return !std::isspace(ch);
		}));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

static void getlineAndTrim(std::ifstream& iFile, std::string& s) {
    getline(iFile, s);
    trim(s);
}

static vector<string> split(string str, char delimiter) {
    vector<string> vals;
    stringstream stream(str);
    string buffer;
 
    while (getline(stream, buffer, delimiter)) {
        if(buffer.length() == 0) continue;
        vals.push_back(buffer);
    }
 
    return vals;
}

static vector<double> splitDouble(string str, char delimiter) {
    vector<double> vals;
    stringstream stream(str);
    string buffer;
 
    while (getline(stream, buffer, delimiter)) {
        if(buffer.length() == 0) continue;
        vals.push_back(stod(buffer));
    }
 
    return vals;
}

BVHReader::BVHReader(string filename): isLoaded(false), filename(filename), root(std::vector<std::unique_ptr<Segment>>()) {};

bool BVHReader::loadFile(){
    string buffer;
    this->iFile = ifstream(this->filename);

    if (this->iFile.fail()) return false;

    while (this->iFile.peek() != EOF){
        getlineAndTrim(this->iFile, buffer);

        if (buffer == "HIERARCHY") this->loadHierarchy();
        if (buffer == "MOTION"){
            if(!(this->loadMotion())){
                cout << "Failed to load motion from .bvh file"<< endl;
            }
        }
    }

    /* fail if no hierarchy is defined */
    if (root.size() == 0) return false;

    this->isLoaded = true;
    return true;
}

bool BVHReader::loadHierarchy(){
    this->channels = 0;
    string buffer;

    stack<std::unique_ptr<Segment>> stack;
    std::unique_ptr<Segment> curr;

    streampos oldpos = this->iFile.tellg(); 

    bool newSeg = false;

    while (this->iFile.peek() != EOF){
        getlineAndTrim(this->iFile, buffer);
        vector<string> words = split(buffer, ' ');

        if(words[0] == "ROOT") {
            if(stack.size() > 0) return false;
            curr = std::make_unique<Segment>(buffer);
            curr->setColor(Eigen::Vector3d(1, 1, 0));
            newSeg = true;
        }
        else if(words[0] == "JOINT") {
            if(curr == NULL) return false;
            stack.push(std::move(curr));
            curr = std::make_unique<Segment>(buffer);
            newSeg = true;
        }
        else if(words[0] == "End") {
            if(curr == NULL) return false;
            stack.push(std::move(curr));
            curr = std::make_unique<Segment>("End Site");
            newSeg = true;
        }
        else if(words[0] == "{"){
            if(newSeg) newSeg = false;
            else return false;
        }

        else if(words[0] == "OFFSET"){
            try {
                auto rot = Eigen::Vector3d(stod(words[1]), stod(words[2]), stod(words[3]));
                curr->setOffset(rot);
            }
            catch (std::exception& e){
                return false;
            }
        }
        else if(words[0] == "CHANNELS"){
            try {
                int channels = stoi(words[1]);
                for (int i = 0; i < channels; i++){
                    string word = words[i+2];

                    if(word == "Xposition") curr->addChannel(Segment::Xposition);
                    else if(word == "Yposition") curr->addChannel(Segment::Yposition);
                    else if(word == "Zposition") curr->addChannel(Segment::Zposition);
                    else if(word == "Xrotation") curr->addChannel(Segment::Xrotation);
                    else if(word == "Yrotation") curr->addChannel(Segment::Yrotation);
                    else if(word == "Zrotation") curr->addChannel(Segment::Zrotation);
                    else return false;
                }

                this->channels += channels;
            }
            catch (std::exception& e){
                return false;
            }
        }
        
        else if(words[0] == "}"){
            if(stack.size() > 0){
                std::unique_ptr<Segment> prev = std::move(stack.top());
                stack.pop();

                auto seg_ptr = std::move(curr);
                prev->addSub(std::move(seg_ptr));

                curr = std::move(prev);
            }
            else{
                auto root_ptr = std::move(curr);
                this->root.push_back(std::move(root_ptr));

                /* this could be an end of hierarchy part */
                oldpos = iFile.tellg();
            }
        }

        else if(words[0] == "MOTION"){
            iFile.seekg(oldpos);
            break;
        }
    }

    return true;
}

bool BVHReader::loadMotion(){
    string buffer;

    try {
        getlineAndTrim(this->iFile, buffer);
        std::vector<string> words = split(buffer, ' ');
        if(words.size() < 2) return false;
        this->frames = stoi(words[1]);

        getlineAndTrim(this->iFile, buffer);
        words = split(buffer, ' ');
        if(words.size() < 3) return false;
        this->frameTime = stod(words[2]);
    }
    catch (std::exception& e){
        return false;
    }

    while (this->iFile.peek() != EOF){
        try {
            getlineAndTrim(this->iFile, buffer);
            std::vector<double> vals = splitDouble(buffer, ' ');
            if (vals.size() != this->channels) return false;
            this->motion.push_back(vals);
        }
        catch (const exception& e){
            return false;
        }
    }
    
    return true;
}

vector<unique_ptr<Segment>> BVHReader::getRoots(){
    return move(this->root);
}

int BVHReader::getChannels(){
    return this->channels;
}

Motion BVHReader::getMotion(){
    return this->motion;
}

int BVHReader::frameSize(){
    return this->frames;
}

double BVHReader::getFrameTime(){
    return this->frameTime;
}

bool BVHReader::loaded(){
    return this->isLoaded;
}
