#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <type_traits>
#include <unordered_map>
#include "octree.hpp"
#include "winged/face.hpp"
#include "winged/vertex.hpp"
#include "triangle.hpp"
#include "ray.hpp"
#include "intersection.hpp"
#include "fwd-winged.hpp"
#include "adjacent-iterator.hpp"
#include "id-map.hpp"
#include "winged/face-intersection.hpp"

#ifdef DILAY_RENDER_OCTREE
#include "mesh.hpp"
#include "renderer.hpp"
#include "color.hpp"
#endif

typedef std::unique_ptr <OctreeNode::Impl> Child;

/** Container for face to insert */
class FaceToInsert {
  public:
    FaceToInsert (const WingedFace& f, const Triangle& t)
      : id               (f.id               ())
      , edge             (f.edge             ())
      , firstIndexNumber (f.firstIndexNumber ())
      , center           (t.center           ())
      , width            (t.maxExtent        ())
      , maximum          (t.maximum          ())
      , minimum          (t.minimum          ())
      {}

    const Id           id;
    WingedEdge* const  edge;
    const unsigned int firstIndexNumber;
    const glm::vec3    center;
    const float        width;
    const glm::vec3    maximum;
    const glm::vec3    minimum;
};

/** Octree node implementation */
struct OctreeNode::Impl {
  IdObject            id;
  OctreeNode          node;
  const glm::vec3     center;
  const float         width;
  std::vector <Child> children;
  const int           depth;
  Faces               faces;
  OctreeNode::Impl*   parent;
  // cf. move constructor

  static constexpr float relativeMinFaceSize = 0.1f;

#ifdef DILAY_RENDER_OCTREE
  Mesh mesh;
#endif

  Impl (const glm::vec3& c, float w, int d, Impl* p) 
    : node (this), center (c), width  (w), depth (d), parent (p) {

#ifdef DILAY_RENDER_OCTREE
      float q = w * 0.5f;
      this->mesh.addVertex (glm::vec3 (-q, -q, -q)); 
      this->mesh.addVertex (glm::vec3 (-q, -q,  q)); 
      this->mesh.addVertex (glm::vec3 (-q,  q, -q)); 
      this->mesh.addVertex (glm::vec3 (-q,  q,  q)); 
      this->mesh.addVertex (glm::vec3 ( q, -q, -q)); 
      this->mesh.addVertex (glm::vec3 ( q, -q,  q)); 
      this->mesh.addVertex (glm::vec3 ( q,  q, -q)); 
      this->mesh.addVertex (glm::vec3 ( q,  q,  q)); 

      this->mesh.addIndex (0); this->mesh.addIndex (1); 
      this->mesh.addIndex (1); this->mesh.addIndex (3); 
      this->mesh.addIndex (3); this->mesh.addIndex (2); 
      this->mesh.addIndex (2); this->mesh.addIndex (0); 

      this->mesh.addIndex (4); this->mesh.addIndex (5); 
      this->mesh.addIndex (5); this->mesh.addIndex (7); 
      this->mesh.addIndex (7); this->mesh.addIndex (6); 
      this->mesh.addIndex (6); this->mesh.addIndex (4); 

      this->mesh.addIndex (1); this->mesh.addIndex (5); 
      this->mesh.addIndex (5); this->mesh.addIndex (7); 
      this->mesh.addIndex (7); this->mesh.addIndex (3); 
      this->mesh.addIndex (3); this->mesh.addIndex (1); 

      this->mesh.addIndex (4); this->mesh.addIndex (6); 
      this->mesh.addIndex (6); this->mesh.addIndex (2); 
      this->mesh.addIndex (2); this->mesh.addIndex (0); 
      this->mesh.addIndex (0); this->mesh.addIndex (4); 

      this->mesh.setPosition (this->center);
      this->mesh.bufferData  ();
#endif
  }
        Impl            (const Impl&) = delete;
  const Impl& operator= (const Impl&) = delete;

  Impl (Impl&& source) 
    : node     (std::move (this))
    , center   (std::move (source.center))
    , width    (std::move (source.width))
    , children (std::move (source.children))
    , depth    (std::move (source.depth))
    , faces    (std::move (source.faces))
    , parent   (std::move (source.parent))
#ifdef DILAY_RENDER_OCTREE
    , mesh     (std::move (source.mesh))

  { this->mesh.bufferData  (); }
#else
  {}
#endif

  void render () {
#ifdef DILAY_RENDER_OCTREE
    this->mesh.renderBegin ();
    glDisable (GL_DEPTH_TEST);
    Renderer :: setColor3 (Color (1.0f, 1.0f, 0.0f));
    glDrawElements (GL_LINES, this->mesh.numIndices (), GL_UNSIGNED_INT, (void*)0);
    glEnable (GL_DEPTH_TEST);
    this->mesh.renderEnd ();

    for (Child& c : this->children) 
      c->render ();
#else
    assert (false);
#endif
  }

  float looseWidth () const { return this->width * 2.0f; }

  bool contains (const glm::vec3& v) {
    const glm::vec3 min = this->center - glm::vec3 (this->width * 0.5f);
    const glm::vec3 max = this->center + glm::vec3 (this->width * 0.5f);
    return glm::all ( glm::lessThanEqual (min, v) )
       &&  glm::all ( glm::lessThanEqual (v, max) );
  }

  bool contains (const FaceToInsert& f) {
    return this->contains (f.center) && f.width <= this->width;
  }

  void makeChildren () {
    assert (this->children.size () == 0);
    float q          = this->width * 0.25f;
    float childWidth = this->width * 0.5f;
    int   childDepth = this->depth + 1;

    auto add = [this,childWidth,childDepth] (const glm::vec3& v) {
      this->children.emplace_back (new Impl (v,childWidth,childDepth,this));
    };
    
    this->children.reserve (8);
    add (this->center + glm::vec3 (-q, -q, -q)); // order is crucial
    add (this->center + glm::vec3 (-q, -q,  q));
    add (this->center + glm::vec3 (-q,  q, -q));
    add (this->center + glm::vec3 (-q,  q,  q));
    add (this->center + glm::vec3 ( q, -q, -q));
    add (this->center + glm::vec3 ( q, -q,  q));
    add (this->center + glm::vec3 ( q,  q, -q));
    add (this->center + glm::vec3 ( q,  q,  q));
  }

  WingedFace& insertIntoChild (const FaceToInsert& f) {
    if (this->children.empty ()) {
      this->makeChildren           ();
      return this->insertIntoChild (f);
    }
    else {
      for (Child& child : this->children) {
        if (child->contains (f))
          return child->insertFace (f);
      }
      assert (false);
    }
  }

  WingedFace& insertFace (const FaceToInsert& f) {
    assert (this->contains (f));

    if (f.width <= this->width * Impl::relativeMinFaceSize) {
      return insertIntoChild (f);
    }
    else {
      this->faces.emplace_back ( f.edge 
                               , f.id 
                               , &this->node
                               , f.firstIndexNumber);
      this->faces.back ().iterator (--this->faces.end ());
      return this->faces.back ();
    }
  }

  bool isEmpty () const {
    return this->faces.empty () && this->children.empty ();
  }

  void deleteFace (const WingedFace& face) {
    this->faces.erase (face.iterator ());
    if (this->isEmpty () && this->parent) {
      this->parent->childEmptyNotification ();
      // don't call anything after calling childEmptyNotification
    }
  }

  void childEmptyNotification () {
    for (Child& c : this->children) {
      if (c->isEmpty () == false) {
        return;
      }
    }
    children.clear ();
    if (this->isEmpty () && this->parent) {
      this->parent->childEmptyNotification ();
      // don't call anything after calling childEmptyNotification
    }
  }

  bool bboxIntersectRay (const Ray& ray) const {
    glm::vec3 invDir  = glm::vec3 (1.0f) / ray.direction ();
    glm::vec3 lower   = this->center - glm::vec3 (this->looseWidth () * 0.5f);
    glm::vec3 upper   = this->center + glm::vec3 (this->looseWidth () * 0.5f);
    glm::vec3 lowerTs = (lower - ray.origin ()) * invDir;
    glm::vec3 upperTs = (upper - ray.origin ()) * invDir;
    glm::vec3 min     = glm::min (lowerTs, upperTs);
    glm::vec3 max     = glm::max (lowerTs, upperTs);

    float tMin = glm::max ( glm::max (min.x, min.y), min.z );
    float tMax = glm::min ( glm::min (max.x, max.y), max.z );

    if (tMax < 0.0f || tMin > tMax)
      return false;
    else
      return true;
  }

  void facesIntersectRay (WingedMesh& mesh, const Ray& ray, WingedFaceIntersection& intersection) {
    for (WingedFace& face : this->faces) {
      Triangle  triangle = face.triangle (mesh);
      glm::vec3 p;

      if (triangle.intersects (ray, p)) {
        intersection.update (glm::distance (ray.origin (), p), p, mesh, face);
      }
    }
  }

  void intersects (WingedMesh& mesh, const Ray& ray, WingedFaceIntersection& intersection) {
    if (this->bboxIntersectRay (ray)) {
      this->facesIntersectRay (mesh,ray,intersection);
      for (Child& c : this->children) 
        c->intersects (mesh,ray,intersection);
    }
  }

  void intersects ( const WingedMesh& mesh, const Sphere& sphere
                       , std::unordered_set<Id>& ids) {
    if (IntersectionUtil :: intersects (sphere, this->node)) {
      for (auto fIt = this->faceIterator (); fIt.isValid (); fIt.next ()) {
        WingedFace& face = fIt.element ();
        if (IntersectionUtil :: intersects (sphere, mesh, face)) {
          ids.insert (face.id ());
        }
      }
      for (Child& c : this->children) {
        c->intersects (mesh, sphere, ids);
      }
    }
  }

  void intersects ( const WingedMesh& mesh, const Sphere& sphere
                       , std::unordered_set<WingedVertex*>& vertices) {
    if (IntersectionUtil :: intersects (sphere, this->node)) {
      for (auto fIt = this->faceIterator (); fIt.isValid (); fIt.next ()) {
        WingedFace& face = fIt.element ();
        for (ADJACENT_VERTEX_ITERATOR (vIt,face)) {
          WingedVertex& vertex = vIt.element ();
          if (IntersectionUtil :: intersects (sphere, mesh, vertex)) {
            vertices.insert (&vertex);
          }
        }
      }
      for (Child& c : this->children) {
        c->intersects (mesh, sphere, vertices);
      }
    }
  }

  unsigned int numFaces () const { return this->faces.size (); }

  OctreeNode* nodeSLOW (const Id& otherId) {
    if (otherId == this->id.id ())
      return &this->node;
    else {
      for (Child& c : this->children) {
        OctreeNode* r = c->nodeSLOW (otherId);
        if (r) return r;
      }
    }
    return nullptr;
  }

  OctreeNodeFaceIterator faceIterator () { 
    return OctreeNodeFaceIterator (*this);
  }

  ConstOctreeNodeFaceIterator faceIterator () const { 
    return ConstOctreeNodeFaceIterator (*this); 
  }
};

OctreeNode :: OctreeNode (OctreeNode::Impl* i) : impl (i) { }

ID             (OctreeNode)
GETTER_CONST   (int, OctreeNode, depth)
GETTER_CONST   (const glm::vec3&, OctreeNode, center)
DELEGATE_CONST (float, OctreeNode, looseWidth)
GETTER_CONST   (float, OctreeNode, width)
DELEGATE3      (void, OctreeNode, intersects, WingedMesh&, const Ray&, WingedFaceIntersection&)
DELEGATE3      (void, OctreeNode, intersects, const WingedMesh&, const Sphere&, std::unordered_set<Id>&)
DELEGATE3      (void, OctreeNode, intersects, const WingedMesh&, const Sphere&, std::unordered_set<WingedVertex*>&)
DELEGATE_CONST (unsigned int, OctreeNode, numFaces)
DELEGATE1      (OctreeNode* , OctreeNode, nodeSLOW, const Id&)
DELEGATE       (OctreeNodeFaceIterator, OctreeNode, faceIterator)
DELEGATE_CONST (ConstOctreeNodeFaceIterator, OctreeNode, faceIterator)

/** Octree class */
struct Octree::Impl {
  Child root;
  IdMapPtr <WingedFace> idMap;

  WingedFace& insertFace (const WingedFace& face, const Triangle& geometry) {
    assert (! this->hasFace (face.id ())); 
    FaceToInsert faceToInsert (face,geometry);
    return this->insertFace (faceToInsert);
  }

  WingedFace& realignFace (const WingedFace& face, const Triangle& geometry, bool* sameNode) {
    assert (this->hasFace   (face.id ())); 
    assert (face.octreeNode ()); 
    OctreeNode* formerNode = face.octreeNode ();

    FaceToInsert faceToInsert (face,geometry);
    this->deleteFace (face);
    WingedFace& newFace = this->insertFace (faceToInsert);

    if (sameNode) {
      *sameNode = formerNode == newFace.octreeNode ();
    }
    return newFace;
  }

  WingedFace& insertFace (const FaceToInsert& faceToInsert) {
    if (! this->root) {
      glm::vec3 rootCenter = (faceToInsert.maximum + faceToInsert.minimum) 
                           * glm::vec3 (0.5f);
      this->root = Child (new OctreeNode::Impl 
          (rootCenter, faceToInsert.width, 0, nullptr));
    }

    if (this->root->contains (faceToInsert)) {
      WingedFace& wingedFace = this->root->insertFace (faceToInsert);
      this->idMap.insert (wingedFace);
      return wingedFace;
    }
    else {
      this->makeParent (faceToInsert);
      return this->insertFace (faceToInsert);
    }
  }

  void deleteFace (const WingedFace& face) {
    assert (face.octreeNode ());
    assert (this->hasFace (face.id ())); 
    this->idMap.erase (face.id ());
    face.octreeNodeRef ().impl->deleteFace (face);
    if (this->root->isEmpty ()) {
      this->root.reset ();
    }
    else {
      this->shrinkRoot ();
    }
  }

  bool hasFace (const Id& id) const { return this->idMap.hasElement (id); }

  WingedFace* face (const Id& id) {
    return this->idMap.element (id);
  }

  void makeParent (const FaceToInsert& f) {
    assert (this->root);

    glm::vec3 parentCenter;
    glm::vec3 rootCenter    = this->root->center;
    float     rootWidth     = this->root->width;
    float     halfRootWidth = this->root->width * 0.5f;
    int       index         = 0;

    if (rootCenter.x < f.center.x) 
      parentCenter.x = rootCenter.x + halfRootWidth;
    else {
      parentCenter.x = rootCenter.x - halfRootWidth;
      index         += 4;
    }
    if (rootCenter.y < f.center.y) 
      parentCenter.y = rootCenter.y + halfRootWidth;
    else {
      parentCenter.y = rootCenter.y - halfRootWidth;
      index         += 2;
    }
    if (rootCenter.z < f.center.z) 
      parentCenter.z = rootCenter.z + halfRootWidth;
    else {
      parentCenter.z = rootCenter.z - halfRootWidth;
      index         += 1;
    }

    OctreeNode::Impl* newRoot = new OctreeNode::Impl ( parentCenter
                                                     , rootWidth * 2.0f
                                                     , this->root->depth - 1
                                                     , nullptr );
    newRoot->makeChildren ();
    newRoot->children [index] = std::move (this->root);
    this->root.reset (newRoot);
    this->root->children [index]->parent = &*this->root;
  }

  void render () { 
#ifdef DILAY_RENDER_OCTREE
    if (this->root) 
      this->root->render ();
#else
    assert (false && "compiled without rendering support for octrees");
#endif
  }

  void intersects (WingedMesh& mesh, const Ray& ray, WingedFaceIntersection& intersection) {
    if (this->root) 
      this->root->intersects (mesh,ray,intersection);
  }

  void intersects (const WingedMesh& mesh, const Sphere& sphere, std::unordered_set<Id>& ids) {
    if (this->root)
      this->root->intersects (mesh,sphere,ids);
  }

  void intersects ( const WingedMesh& mesh, const Sphere& sphere
                  , std::unordered_set<WingedVertex*>& vertices) {
    if (this->root)
      this->root->intersects (mesh,sphere,vertices);
  }

  void reset () { 
    this->idMap.reset  ();
    this->root.release (); 
  }

  void initRoot (const glm::vec3& center, float width) {
    assert (this->root == false);
    this->root = Child (new OctreeNode::Impl (center, width, 0, nullptr));
  }

  void shrinkRoot () {
    if (this->root && this->root->faces.empty    () == true 
                   && this->root->children.empty () == false) {
      int singleNonEmptyChildIndex = -1;
      for (int i = 0; i < 8; i++) {
        const Child& c = this->root->children [i];
        if (c->isEmpty () == false) {
          if (singleNonEmptyChildIndex == -1) 
            singleNonEmptyChildIndex = i;
          else
            return;
        }
      }
      if (singleNonEmptyChildIndex != -1) {
        Child newRoot      = std::move (this->root->children [singleNonEmptyChildIndex]);
        this->root         = std::move (newRoot);
        this->root->parent = nullptr;
        this->shrinkRoot ();
      }
    }
  }

  OctreeNode& nodeSLOW (const Id& id) {
    if (this->root) {
      OctreeNode* result = this->root->nodeSLOW (id);
      if (result) 
        return *result;
      else 
        assert (false);
    }
    assert (false);
  }

  OctreeFaceIterator      faceIterator ()       { return OctreeFaceIterator      (*this); }
  ConstOctreeFaceIterator faceIterator () const { return ConstOctreeFaceIterator (*this); }
  OctreeNodeIterator      nodeIterator ()       { return OctreeNodeIterator      (*this); }
  ConstOctreeNodeIterator nodeIterator () const { return ConstOctreeNodeIterator (*this); }
};

DELEGATE_BIG3   (Octree)
DELEGATE2       (WingedFace& , Octree, insertFace, const WingedFace&, const Triangle&)
DELEGATE3       (WingedFace& , Octree, realignFace, const WingedFace&, const Triangle&, bool*)
DELEGATE1       (void        , Octree, deleteFace, const WingedFace&)
DELEGATE1_CONST (bool        , Octree, hasFace, const Id&)
DELEGATE1       (WingedFace* , Octree, face, const Id&)
DELEGATE        (void, Octree, render)
DELEGATE3       (void, Octree, intersects, WingedMesh&, const Ray&, WingedFaceIntersection&)
DELEGATE3       (void, Octree, intersects, const WingedMesh&, const Sphere&, std::unordered_set<Id>&)
DELEGATE3       (void, Octree, intersects, const WingedMesh&, const Sphere&, std::unordered_set<WingedVertex*>&)
DELEGATE        (void, Octree, reset)
DELEGATE2       (void, Octree, initRoot, const glm::vec3&, float)
DELEGATE        (void, Octree, shrinkRoot)
DELEGATE1       (OctreeNode&, Octree, nodeSLOW, const Id&)
DELEGATE        (OctreeFaceIterator, Octree, faceIterator)
DELEGATE_CONST  (ConstOctreeFaceIterator, Octree, faceIterator)
DELEGATE        (OctreeNodeIterator, Octree, nodeIterator)
DELEGATE_CONST  (ConstOctreeNodeIterator, Octree, nodeIterator)

/** Internal template for iterators over all faces of a node */
template <bool isConstant>
struct OctreeNodeFaceIteratorTemplate {
  typedef typename 
    std::conditional <isConstant, const OctreeNode::Impl, OctreeNode::Impl>::type 
    T_OctreeNodeImpl;
  typedef typename 
    std::conditional <isConstant, std::list <WingedFace>::const_iterator
                                , std::list <WingedFace>::iterator>::type
    T_Iterator;
  typedef typename 
    std::conditional <isConstant, const WingedFace, WingedFace>::type
    T_Element;

  T_OctreeNodeImpl& octreeNode;
  T_Iterator        iterator;

  OctreeNodeFaceIteratorTemplate (T_OctreeNodeImpl& n) 
    : octreeNode (n) 
    , iterator (n.faces.begin ())
  {}

  bool isValid () const { 
    return this->iterator != this->octreeNode.faces.end ();
  }

  T_Element& element () const {
    assert (this->isValid ());
    return * this->iterator;
  }

  void next () {
    assert (this->isValid ());
    this->iterator++;
  }

  int depth () const { 
    return this->octreeNode.depth;
  }
};

DELEGATE1_BIG6 (OctreeNodeFaceIterator,OctreeNode::Impl&)
DELEGATE_CONST (bool       , OctreeNodeFaceIterator, isValid)
DELEGATE_CONST (WingedFace&, OctreeNodeFaceIterator, element)
DELEGATE       (void       , OctreeNodeFaceIterator, next)
DELEGATE_CONST (int        , OctreeNodeFaceIterator, depth)

DELEGATE1_BIG6 (ConstOctreeNodeFaceIterator,const OctreeNode::Impl&)
DELEGATE_CONST (bool             , ConstOctreeNodeFaceIterator, isValid)
DELEGATE_CONST (const WingedFace&, ConstOctreeNodeFaceIterator, element)
DELEGATE       (void             , ConstOctreeNodeFaceIterator, next)
DELEGATE_CONST (int              , ConstOctreeNodeFaceIterator, depth)

/** Internal template for iterators over all faces of an octree */
template <bool isConstant>
struct OctreeFaceIteratorTemplate {
  typedef typename 
    std::conditional <isConstant, const Octree::Impl, Octree::Impl>::type 
    T_OctreeImpl;
  typedef typename 
    std::conditional <isConstant, const WingedFace, WingedFace>::type
    T_Element;
  typedef 
    OctreeNodeFaceIteratorTemplate <isConstant>
    T_OctreeNodeFaceIterator;

  std::list <T_OctreeNodeFaceIterator> faceIterators;

  OctreeFaceIteratorTemplate (T_OctreeImpl& octree) {
    if (octree.root) {
      T_OctreeNodeFaceIterator rootIterator (*octree.root);
      this->faceIterators.push_back (rootIterator);
      this->skipEmptyNodes ();
    }
  }

  void skipEmptyNodes () {
    if (this->faceIterators.size () > 0) {
      T_OctreeNodeFaceIterator& current = this->faceIterators.front ();
      if (! current.isValid ()) {
        for (auto & c : current.octreeNode.children) {
          this->faceIterators.push_back (T_OctreeNodeFaceIterator (*c.get ()));
        }
        this->faceIterators.pop_front ();
        this->skipEmptyNodes ();
      }
    }
  }

  bool isValid () const { return this->faceIterators.size () > 0; }

  T_Element & element () const {
    assert (this->isValid ());
    return this->faceIterators.front ().element ();
  }

  void next () { 
    assert (this->isValid ());
    this->faceIterators.front ().next ();
    this->skipEmptyNodes ();
  }

  int depth () const {
    assert (this->isValid ());
    return this->faceIterators.front ().octreeNode.depth;
  }
};

DELEGATE1_BIG6 (OctreeFaceIterator,Octree::Impl&)
DELEGATE_CONST (bool       , OctreeFaceIterator, isValid)
DELEGATE_CONST (WingedFace&, OctreeFaceIterator, element)
DELEGATE       (void       , OctreeFaceIterator, next)
DELEGATE_CONST (int        , OctreeFaceIterator, depth)

DELEGATE1_BIG6 (ConstOctreeFaceIterator,const Octree::Impl&)
DELEGATE_CONST (bool             , ConstOctreeFaceIterator, isValid)
DELEGATE_CONST (const WingedFace&, ConstOctreeFaceIterator, element)
DELEGATE       (void             , ConstOctreeFaceIterator, next)
DELEGATE_CONST (int              , ConstOctreeFaceIterator, depth)

/** Internal template for iterators over all nodes of an octree */
template <bool isConstant>
struct OctreeNodeIteratorTemplate {
  typedef typename 
    std::conditional <isConstant, const Octree::Impl, Octree::Impl>::type 
    T_OctreeImpl;
  typedef typename 
    std::conditional <isConstant, const OctreeNode,OctreeNode>::type 
    T_OctreeNode;
  typedef typename 
    std::conditional <isConstant, const OctreeNode::Impl,OctreeNode::Impl>::type 
    T_OctreeNodeImpl;

  std::list <T_OctreeNodeImpl*> nodes;

  OctreeNodeIteratorTemplate (T_OctreeImpl& octree) {
    if (octree.root) {
      this->nodes.push_back (octree.root.get ());
    }
  }

  T_OctreeNode& element () const {
    assert (this->isValid ());
    return this->nodes.front ()->node;
  }

  bool isValid () const { return this->nodes.size () > 0; }

  void next () { 
    assert (this->isValid ());
    for (auto & c : this->nodes.front ()->children) {
      this->nodes.push_back (c.get ());
    }
    this->nodes.pop_front ();
  }
};

DELEGATE1_BIG6 (OctreeNodeIterator, Octree::Impl&)    
DELEGATE_CONST (bool              , OctreeNodeIterator, isValid)
DELEGATE_CONST (OctreeNode&       , OctreeNodeIterator, element)
DELEGATE       (void              , OctreeNodeIterator, next)

DELEGATE1_BIG6 (ConstOctreeNodeIterator, const Octree::Impl&)    
DELEGATE_CONST (bool              , ConstOctreeNodeIterator, isValid)
DELEGATE_CONST (const OctreeNode& , ConstOctreeNodeIterator, element)
DELEGATE       (void              , ConstOctreeNodeIterator, next)
